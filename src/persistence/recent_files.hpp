#pragma once

#include <filesystem>
#include <vector>

namespace openphysx {

// The last projects opened or saved, newest first. Missing files stay in the list
// so the menu can show them as unavailable.
class RecentFiles
{
public:
    static constexpr int kLimit = 10;

    RecentFiles();
    explicit RecentFiles(std::filesystem::path store);

    void Load();
    void Add(const std::filesystem::path& path);
    const std::vector<std::filesystem::path>& Entries() const { return entries_; }
    static bool Exists(const std::filesystem::path& path);

private:
    void Save() const;

    std::filesystem::path store_;
    std::vector<std::filesystem::path> entries_;
};

} // namespace openphysx
