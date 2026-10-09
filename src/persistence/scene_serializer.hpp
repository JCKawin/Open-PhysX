#pragma once

#include "ecs/load_report.hpp"
#include "ecs/scene.hpp"
#include "persistence/errors.hpp"

#include <nlohmann/json.hpp>

#include <expected>

namespace openphysx {

// Entities are written in creation order. Object keys are sorted by the JSON library.
nlohmann::json WriteScene(const Scene& scene);

// Builds a temporary scene and swaps it into `scene` only after the document is readable.
// Unknown components are kept and mentioned in the report. Dangling ids are cleared.
std::expected<void, LoadError> ReadScene(const nlohmann::json& json, Scene& scene, LoadReport& report);

} // namespace openphysx
