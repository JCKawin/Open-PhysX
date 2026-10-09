#pragma once

#include "persistence/errors.hpp"
#include "persistence/project.hpp"

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace openphysx {

// MyRobot.opx -> MyRobot.opx.data
std::filesystem::path SidecarDir(const std::filesystem::path& project);

// Copies a file into <project>.opx.data/assets and records a relative path plus content hash.
// The copy happens before the project file is saved.
std::expected<AssetRecord, SaveError> ImportAsset(
    const std::filesystem::path& project, const std::filesystem::path& source, UUID id);

std::expected<void, SaveError> WriteCache(
    const std::filesystem::path& project, std::string_view name, std::span<const float> samples);

// A missing cache is NotFound and is not a project error. The field can be regenerated.
std::expected<std::vector<float>, LoadError> ReadCache(const std::filesystem::path& project, std::string_view name);

} // namespace openphysx
