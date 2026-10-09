#pragma once

#include "persistence/errors.hpp"
#include "persistence/project.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <filesystem>

namespace openphysx {

// Rewrites a project document up to Project::kFormatVersion.
// `source_version` is the version stored in the file before migration.
std::expected<nlohmann::json, LoadError> MigrateProject(nlohmann::json document, int& source_version);

// Copies the untouched original to <stem>.v<old>.bak before the first save of a migrated project.
std::expected<void, SaveError> ArchiveMigratedOriginal(const std::filesystem::path& path, int old_version);

} // namespace openphysx
