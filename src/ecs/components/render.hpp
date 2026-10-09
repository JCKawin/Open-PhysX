#pragma once

#include "core/Types.h"
#include "ecs/uuid.hpp"

namespace openphysx {

// Saved mesh reference. The GPU buffer lives in MeshGpuHandle and is never saved.
struct MeshRendererComponent
{
    UUID meshAsset = kNullUuid;
    UUID materialAsset = kNullUuid;
    bool castShadow = true;
};

// Optional per-entity material values. Empty means the asset material is used.
struct MaterialOverrideComponent
{
    Rgb albedo{1.0f, 1.0f, 1.0f};
    float roughness = 0.5f;
    float metallic = 0.0f;
};

inline bool operator==(const MeshRendererComponent& a, const MeshRendererComponent& b)
{
    return a.meshAsset == b.meshAsset && a.materialAsset == b.materialAsset && a.castShadow == b.castShadow;
}

inline bool operator==(const MaterialOverrideComponent& a, const MaterialOverrideComponent& b)
{
    return a.albedo == b.albedo && a.roughness == b.roughness && a.metallic == b.metallic;
}

} // namespace openphysx
