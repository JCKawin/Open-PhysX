#include "persistence/recovery.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <fstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <csignal>
#include <unistd.h>
#endif

namespace openphysx {
namespace {

std::filesystem::path g_marker;

void write_marker()
{
    if (g_marker.empty())
        return;
#if defined(_WIN32)
    HANDLE file = CreateFileW(g_marker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    const char text[] = "crash\n";
    WriteFile(file, text, sizeof(text) - 1, &written, nullptr);
    CloseHandle(file);
#else
    const int fd = ::open(g_marker.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0)
    {
        const char text[] = "crash\n";
        (void)::write(fd, text, sizeof(text) - 1);
        ::close(fd);
    }
#endif
}

#if defined(_WIN32)
LONG WINAPI crash_filter(EXCEPTION_POINTERS*)
{
    write_marker();
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
void crash_signal(int)
{
    write_marker();
    _exit(128);
}
#endif

void on_terminate()
{
    write_marker();
    std::abort();
}

std::uint32_t read_lock_pid(const std::filesystem::path& lock, bool& ok)
{
    ok = false;
    std::ifstream input(lock);
    if (!input)
        return 0;
    unsigned long pid = 0;
    input >> pid;
    ok = static_cast<bool>(input);
    return static_cast<std::uint32_t>(pid);
}

} // namespace

bool ProcessAlive(std::uint32_t pid)
{
    if (pid == 0)
        return false;
#if defined(_WIN32)
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr)
        return false;
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(process, &code) && code == STILL_ACTIVE;
    CloseHandle(process);
    return alive;
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

void WriteSessionLock(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    std::ofstream output(directory / "session.lock", std::ios::trunc);
    if (!output)
        return;
#if defined(_WIN32)
    output << GetCurrentProcessId() << "\n";
#else
    output << ::getpid() << "\n";
#endif
    const auto now = std::chrono::system_clock::now();
    output << std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() << "\n";
}

void RemoveSessionLock(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::remove(directory / "session.lock", error);
}

void WriteCleanExit(const std::filesystem::path& directory)
{
    std::ofstream output(directory / "clean.exit", std::ios::trunc);
    if (output)
        output << "ok\n";
}

void WriteCrashMarker(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    g_marker = directory / "crash.marker";
    write_marker();
}

void InstallCrashHandler(const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    g_marker = directory / "crash.marker";
    std::set_terminate(on_terminate);
#if defined(_WIN32)
    SetUnhandledExceptionFilter(crash_filter);
#else
    std::signal(SIGSEGV, crash_signal);
    std::signal(SIGABRT, crash_signal);
#endif
}

std::vector<CrashedSession> FindCrashedSessions(
    const std::filesystem::path& autosave_root, const std::filesystem::path& current_session)
{
    std::vector<CrashedSession> found;
    std::error_code error;
    if (!std::filesystem::exists(autosave_root, error))
        return found;

    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(autosave_root, error))
    {
        if (!entry.is_directory())
            continue;
        if (std::filesystem::equivalent(entry.path(), current_session, error))
            continue;

        std::filesystem::path newest;
        std::filesystem::file_time_type newest_time{};
        bool any = false;
        for (const std::filesystem::directory_entry& file : std::filesystem::directory_iterator(entry.path(), error))
        {
            if (file.path().extension() != ".opx")
                continue;
            const auto stamp = file.last_write_time();
            if (!any || stamp > newest_time)
            {
                newest = file.path();
                newest_time = stamp;
                any = true;
            }
        }
        if (!any)
            continue;

        const bool clean = std::filesystem::exists(entry.path() / "clean.exit", error);
        bool lock_ok = false;
        const std::uint32_t pid = read_lock_pid(entry.path() / "session.lock", lock_ok);
        const bool dead = !lock_ok || !ProcessAlive(pid);
        if (clean || !dead)
            continue;

        CrashedSession crashed;
        crashed.directory = entry.path();
        crashed.autosave = newest;
        std::ifstream origin(entry.path() / "origin.txt");
        std::string origin_line;
        if (std::getline(origin, origin_line) && !origin_line.empty())
        {
            crashed.origin = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(origin_line.c_str())));
            crashed.name = crashed.origin.filename().string();
        }
        else
        {
            crashed.name = newest.stem().string();
        }
        crashed.when = newest.filename().string();
        found.push_back(std::move(crashed));
    }

    std::sort(found.begin(), found.end(), [](const CrashedSession& left, const CrashedSession& right) {
        return left.autosave.filename().string() > right.autosave.filename().string();
    });
    return found;
}

} // namespace openphysx
