#pragma once

#include "ecs/load_report.hpp"
#include "persistence/errors.hpp"
#include "persistence/project.hpp"

#include <expected>
#include <filesystem>

namespace openphysx {

class ProjectFile
{
public:
    static std::expected<LoadReport, LoadError> Load(const std::filesystem::path& path, Project& out);
    static std::expected<std::string, SaveError> Serialize(const Project& project);
    static std::expected<void, SaveError> Save(const std::filesystem::path& path, const Project& project);
};

std::string CurrentUtcTimestamp();

} // namespace openphysx
