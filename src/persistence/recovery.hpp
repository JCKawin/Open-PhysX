#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace openphysx {

struct CrashedSession
{
    std::filesystem::path directory;
    std::filesystem::path autosave;
    std::filesystem::path origin;
    std::string name;
    std::string when;
};

void WriteSessionLock(const std::filesystem::path& directory);
void RemoveSessionLock(const std::filesystem::path& directory);
void WriteCleanExit(const std::filesystem::path& directory);
void WriteCrashMarker(const std::filesystem::path& directory);
void InstallCrashHandler(const std::filesystem::path& directory);

// Newest autosave first. The running session directory is ignored.
// A session counts as crashed when its lock names a dead process, or autosaves
// were left behind without a clean-exit marker.
std::vector<CrashedSession> FindCrashedSessions(
    const std::filesystem::path& autosave_root, const std::filesystem::path& current_session);

bool ProcessAlive(std::uint32_t pid);

} // namespace openphysx
