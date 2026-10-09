#pragma once

#include "core/Types.h"
#include "ecs/scene.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace openphysx {

struct AssetRecord
{
    UUID id = kNullUuid;
    std::string path;
    std::string hash;
};

struct ProjectCamera
{
    Quat rotation{};
    Vec3 offset{0.0f, -1.0f, 0.0f};
    float distance = 6.403124f;
    float fovy_deg = 45.0f;
    bool orthographic = false;
};

// The authored project. Playback time and the runtime scene are not part of it.
struct Project
{
    static constexpr int kFormatVersion = 1;
    static constexpr const char* kAppVersion = "0.1.0";
    static constexpr std::uintmax_t kMaxFileBytes = 64ull * 1024ull * 1024ull;

    Scene scene{};
    Vec3 gravity{0.0f, -9.81f, 0.0f};
    float timestep = 0.001f;
    float duration = 10.0f;
    float playback_speed = 1.0f;
    bool loop = true;
    bool show_grid = true;
    bool demo_motion = true;
    int grid_slices = 20;
    float grid_spacing = 1.0f;
    Rgb clear_color{0.12f, 0.12f, 0.14f};
    ProjectCamera camera{};
    std::vector<UUID> selection;
    std::string layout_ini;
    std::vector<AssetRecord> assets;
    std::string created_utc;
    std::string modified_utc;
    int format_version = kFormatVersion;
    int source_version = kFormatVersion;
};

} // namespace openphysx
