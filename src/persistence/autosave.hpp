#pragma once

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace openphysx {

// Writes project snapshots off the main thread. The caller builds the bytes.
// The worker never touches the live scene, OpenGL, or the window.
class Autosave
{
public:
    Autosave();
    explicit Autosave(std::filesystem::path session_dir);
    ~Autosave();

    Autosave(const Autosave&) = delete;
    Autosave& operator=(const Autosave&) = delete;

    void SetInterval(std::chrono::milliseconds interval);
    const std::filesystem::path& SessionDir() const { return directory_; }

    // Main thread. Serializes only when the project is dirty, the user is idle, and the interval has elapsed.
    void Tick(bool dirty, bool idle, const std::function<std::string()>& snapshot, std::string project_name);
    void WaitIdle();
    void DeleteAutosaves();
    bool ConsumeNotice();

private:
    void Run();
    void Prune() const;

    std::filesystem::path directory_;
    std::chrono::milliseconds interval_{std::chrono::minutes(2)};
    std::chrono::steady_clock::time_point last_{};
    bool started_ = false;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<std::string> pending_;
    std::string pending_name_;
    bool stop_ = false;
    bool busy_ = false;
    bool notice_ = false;
    int serial_ = 0;
    std::thread thread_;
};

} // namespace openphysx
