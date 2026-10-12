#pragma once

// B3.1 — Import result/options types and axis conversion helper.
// Layering rule: no raylib, no ImGui here.
// All importers return an ImportResult; they never touch the window.

#include "core/Math.h"
#include "core/Mesh.h"
#include "core/Types.h"
#include "ecs/components/core.hpp"

#include <optional>
#include <string>

namespace openphysx {

// Options passed in by the UI for every import call.
struct ImportOptions
{
    float scale = 1.0f;
    bool  z_up  = true;  // when true, apply z_up_to_y_up() at the import boundary
};

// Result returned by every importer function.
// On success: mesh is set, error is empty.
// On failure: mesh is empty, error explains why.
// transform is always identity in Epic B (object transform = identity, positions baked).
struct ImportResult
{
    std::optional<Mesh> mesh;
    TransformComponent  transform{}; // identity: position (0,0,0), rotation identity, scale (1,1,1)
    std::string         error;
};

// ---------------------------------------------------------------------------
// Axis conversion: Z-up (CAD / Blender export default internal) → Y-up
//
// Matrix (applied to column vector):
//   [ 1  0  0 ]   x → x
//   [ 0  0  1 ] · y → z
//   [ 0 -1  0 ]   z → -y
//
// Determinant = +1, so CCW winding is preserved.
// Apply once to all positions at the import boundary before they enter Mesh.
// Never re-apply inside the app.
// ---------------------------------------------------------------------------
inline Vec3 z_up_to_y_up(Vec3 p)
{
    // (x, y, z) → (x, z, -y)
    return Vec3{ p.x, p.z, -p.y };
}

} // namespace openphysx
