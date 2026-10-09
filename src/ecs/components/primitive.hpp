#pragma once

#include "core/Types.h"

namespace openphysx {

// The viewport still draws a box. Mesh assets are not in the engine yet.
struct PrimitiveBoxComponent
{
    Vec3 size{2.0f, 2.0f, 2.0f};
    Rgb color{0.15f, 0.35f, 0.85f};
};

inline bool operator==(const PrimitiveBoxComponent& a, const PrimitiveBoxComponent& b)
{
    return a.size == b.size && a.color == b.color;
}

} // namespace openphysx
