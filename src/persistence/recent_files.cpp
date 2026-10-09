#include "persistence/recent_files.hpp"

#include "persistence/app_data.hpp"

#include <fstream>

namespace openphysx {

RecentFiles::RecentFiles()
    : RecentFiles(std::filesystem::path{})
{
}

RecentFiles::RecentFiles(std::filesystem::path store)
    : store_(std::move(store))
{
    if (store_.empty())
        store_ = AppDataDir() / "recent.txt";
}

void RecentFiles::Load()
{
    entries_.clear();
    std::ifstream input(store_);
    if (!input)
        return;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty())
            continue;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        entries_.push_back(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(line.c_str()))));
        if (static_cast<int>(entries_.size()) >= kLimit)
            break;
    }
}

void RecentFiles::Add(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> next;
    next.push_back(path);
    for (const std::filesystem::path& entry : entries_)
    {
        if (entry == path)
            continue;
        next.push_back(entry);
        if (static_cast<int>(next.size()) >= kLimit)
            break;
    }
    entries_ = std::move(next);
    Save();
}

bool RecentFiles::Exists(const std::filesystem::path& path)
{
    std::error_code error;
    return std::filesystem::exists(path, error) && !error;
}

void RecentFiles::Save() const
{
    std::error_code error;
    std::filesystem::create_directories(store_.parent_path(), error);
    std::ofstream output(store_, std::ios::binary | std::ios::trunc);
    if (!output)
        return;
    for (const std::filesystem::path& entry : entries_)
    {
        const std::u8string text = entry.u8string();
        output.write(reinterpret_cast<const char*>(text.data()), static_cast<std::streamsize>(text.size()));
        output.put('\n');
    }
}

} // namespace openphysx
