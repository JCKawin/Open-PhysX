#include "persistence/atomic_write.hpp"

#include <atomic>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#endif

namespace openphysx {
namespace {

std::atomic<AtomicFailPoint> g_fail_point{AtomicFailPoint::None};

SaveError io_error(std::string message)
{
    return SaveError{SaveError::Kind::Io, std::move(message)};
}

#if defined(_WIN32)

SaveError from_win32(DWORD error, const std::string& action)
{
    if (error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL)
        return SaveError{SaveError::Kind::DiskFull, action + " failed because the disk is full."};
    if (error == ERROR_ACCESS_DENIED || error == ERROR_WRITE_PROTECT || error == ERROR_SHARING_VIOLATION)
        return SaveError{SaveError::Kind::Permission, action + " was denied."};
    return io_error(action + " failed (Windows error " + std::to_string(error) + ").");
}

bool write_all(HANDLE file, std::string_view bytes)
{
    const char* data = bytes.data();
    std::size_t remaining = bytes.size();
    while (remaining > 0)
    {
        DWORD chunk = 0;
        const DWORD request = remaining > 1u << 20 ? static_cast<DWORD>(1u << 20) : static_cast<DWORD>(remaining);
        if (!WriteFile(file, data, request, &chunk, nullptr))
            return false;
        if (chunk == 0)
            return false;
        data += chunk;
        remaining -= chunk;
    }
    return true;
}

std::expected<void, SaveError> write_temp(const std::filesystem::path& tmp, std::string_view bytes)
{
    HANDLE file = CreateFileW(
        tmp.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return std::unexpected(from_win32(GetLastError(), "Creating the temporary file"));

    const bool wrote = write_all(file, bytes);
    const DWORD write_error = wrote ? ERROR_SUCCESS : GetLastError();
    const bool flushed = wrote && FlushFileBuffers(file);
    const DWORD flush_error = flushed ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);

    if (!wrote)
    {
        std::error_code ignored;
        std::filesystem::remove(tmp, ignored);
        return std::unexpected(from_win32(write_error, "Writing the temporary file"));
    }
    if (!flushed)
    {
        std::error_code ignored;
        std::filesystem::remove(tmp, ignored);
        return std::unexpected(from_win32(flush_error, "Flushing the temporary file"));
    }
    return {};
}

std::expected<void, SaveError> move_file(const std::filesystem::path& from, const std::filesystem::path& to, const char* action)
{
    if (MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return {};
    return std::unexpected(from_win32(GetLastError(), action));
}

#else

std::expected<void, SaveError> write_temp(const std::filesystem::path& tmp, std::string_view bytes)
{
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        return std::unexpected(io_error("Creating the temporary file failed."));
    std::size_t written = 0;
    while (written < bytes.size())
    {
        const ssize_t chunk = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (chunk < 0)
        {
            ::close(fd);
            std::error_code ignored;
            std::filesystem::remove(tmp, ignored);
            return std::unexpected(io_error("Writing the temporary file failed."));
        }
        written += static_cast<std::size_t>(chunk);
    }
    if (::fsync(fd) != 0)
    {
        ::close(fd);
        std::error_code ignored;
        std::filesystem::remove(tmp, ignored);
        return std::unexpected(io_error("Flushing the temporary file failed."));
    }
    ::close(fd);
    return {};
}

std::expected<void, SaveError> move_file(const std::filesystem::path& from, const std::filesystem::path& to, const char*)
{
    std::error_code error;
    std::filesystem::rename(from, to, error);
    if (!error)
        return {};
    return std::unexpected(io_error("Renaming the project file failed."));
}

#endif

} // namespace

void SetAtomicFailPoint(AtomicFailPoint point)
{
    g_fail_point.store(point);
}

std::expected<void, SaveError> AtomicWrite(const std::filesystem::path& target, std::string_view bytes)
{
    std::error_code error;
    if (!target.parent_path().empty())
        std::filesystem::create_directories(target.parent_path(), error);
    if (error)
        return std::unexpected(io_error("The project folder could not be created."));

    const std::filesystem::path tmp = target.wstring() + L".tmp";
    const std::filesystem::path bak = target.wstring() + L".bak";

    if (auto written = write_temp(tmp, bytes); !written)
        return written;

    if (g_fail_point.load() == AtomicFailPoint::AfterTempWrite)
    {
        std::filesystem::remove(tmp, error);
        return std::unexpected(io_error("Saving stopped before the file was replaced."));
    }

    const bool had_target = std::filesystem::exists(target);
    if (had_target)
    {
        if (auto moved = move_file(target, bak, "Moving the previous project to a backup"); !moved)
        {
            std::filesystem::remove(tmp, error);
            return moved;
        }
    }

    if (auto moved = move_file(tmp, target, "Replacing the project file"); !moved)
    {
        if (had_target)
        {
            std::error_code ignored;
            std::filesystem::rename(bak, target, ignored);
        }
        std::filesystem::remove(tmp, error);
        return moved;
    }
    return {};
}

} // namespace openphysx
