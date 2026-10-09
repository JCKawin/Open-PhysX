#include "persistence/autosave.hpp"

#include "persistence/app_data.hpp"
#include "persistence/atomic_write.hpp"

#include <algorithm>
#include <chrono>
#include <vector>
#include <cstdint>
#include <ctime>

namespace openphysx {
namespace {

std::string stamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#if defined(_WIN32)
    gmtime_s(&parts, &now);
#else
    gmtime_r(&now, &parts);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &parts);
    return buffer;
}

std::string safe_name(std::string name)
{
    if (name.empty())
        name = "Untitled";
    for (char& character : name)
    {
        const bool ok = (character >= '0' && character <= '9') || (character >= 'A' && character <= 'Z') ||
                        (character >= 'a' && character <= 'z') || character == '-' || character == '_';
        if (!ok)
            character = '_';
    }
    return name;
}

} // namespace

Autosave::Autosave()
    : Autosave(std::filesystem::path{})
{
}

Autosave::Autosave(std::filesystem::path session_dir)
    : directory_(std::move(session_dir))
{
    if (directory_.empty())
    {
        const auto ticks = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        directory_ = AppDataDir() / "autosave" / std::to_string(ticks);
    }
    thread_ = std::thread([this] { Run(); });
}

Autosave::~Autosave()
{
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

void Autosave::SetInterval(std::chrono::milliseconds interval)
{
    interval_ = interval;
}

void Autosave::Tick(bool dirty, bool idle, const std::function<std::string()>& snapshot, std::string project_name)
{
    if (!dirty || !idle || !snapshot)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (started_ && now - last_ < interval_)
        return;
    started_ = true;
    last_ = now;
    std::string bytes = snapshot();
    if (bytes.empty())
        return;
    {
        std::lock_guard lock(mutex_);
        pending_ = std::move(bytes);
        pending_name_ = std::move(project_name);
    }
    cv_.notify_all();
}

void Autosave::WaitIdle()
{
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [&] { return !pending_ && !busy_; });
}

void Autosave::DeleteAutosaves()
{
    WaitIdle();
    std::error_code error;
    if (!std::filesystem::exists(directory_, error))
        return;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory_, error))
    {
        if (entry.path().extension() == ".opx")
            std::filesystem::remove(entry.path(), error);
    }
}

bool Autosave::ConsumeNotice()
{
    std::lock_guard lock(mutex_);
    const bool notice = notice_;
    notice_ = false;
    return notice;
}

void Autosave::Run()
{
    for (;;)
    {
        std::string bytes;
        std::string name;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [&] { return stop_ || pending_.has_value(); });
            if (stop_ && !pending_)
                return;
            bytes = std::move(*pending_);
            pending_.reset();
            name = pending_name_;
            busy_ = true;
        }

        const std::filesystem::path file = directory_ / (safe_name(name) + "_" + stamp() + "_" + std::to_string(++serial_) + ".opx");
        const auto written = AtomicWrite(file, bytes);
        Prune();

        {
            std::lock_guard lock(mutex_);
            busy_ = false;
            if (written)
                notice_ = true;
        }
        cv_.notify_all();
    }
}

void Autosave::Prune() const
{
    std::error_code error;
    std::vector<std::filesystem::directory_entry> files;
    if (!std::filesystem::exists(directory_, error))
        return;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory_, error))
    {
        if (entry.path().extension() == ".opx")
            files.push_back(entry);
    }
    std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) {
        return left.last_write_time() > right.last_write_time();
    });
    for (std::size_t index = 5; index < files.size(); ++index)
        std::filesystem::remove(files[index].path(), error);
}

} // namespace openphysx
