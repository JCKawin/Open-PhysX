#pragma once

#include "core/Types.h"

namespace openphysx {

// The viewport still draws a box. Mesh assets are not in the engine yet.
struct PrimitiveBoxComponent
{
    Vec3 size{2.0f, 2.0f, 2.0f};
    Rgb color{0.15f, 0.35f, 0.85f};
    uint32_t mesh_id = 0; // 0 = default box primitive, >0 = scene mesh library id (1-based)
};

inline bool operator==(const PrimitiveBoxComponent& a, const PrimitiveBoxComponent& b)
{
    return a.size == b.size && a.color == b.color && a.mesh_id == b.mesh_id;
}

} // namespace openphysx
