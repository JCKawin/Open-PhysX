#pragma once

// B1 — Mesh component.
// No raylib or ImGui includes here (layering rule: core/ is renderer-free).

#include "core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace openphysx {

// Axis-aligned bounding box.
struct Aabb
{
    Vec3 min{};
    Vec3 max{};
};

// Plain triangle mesh. Immutable once imported or created.
// Indices are 3-per-triangle, CCW winding seen from outside (Y-up, right-handed).
// Normals are per-vertex; compute_vertex_normals must be called after construction.
// This type must never be included from renderer or UI code via a public header that
// pulls in raylib/imgui — keep it in core/.
struct Mesh
{
    std::vector<Vec3>     positions; // one per vertex
    std::vector<uint32_t> indices;   // 3 per triangle
    std::vector<Vec3>     normals;   // 1 per vertex (same count as positions, or empty before compute)

    std::size_t triangle_count() const { return indices.size() / 3; }

    // Returns an empty string when the mesh is internally consistent.
    // Returns an error reason otherwise.
    // Conditions checked:
    //   indices.size() % 3 == 0
    //   every index < positions.size()
    //   normals.size() == positions.size() OR normals.empty()
    std::string validate() const;

    bool operator==(const Mesh& other) const = default;
};

// Accumulates face-area-weighted normals, normalizes per vertex.
// Zero-area face normals are skipped. Isolated vertices fall back to (0,1,0).
// Call once after construction; never call from the renderer.
void compute_vertex_normals(Mesh& m);

// 24-vertex cube (4 vertices per face, not shared), 12 triangles.
// CCW from outside, +Y up. size is the full edge length along each axis, centred at origin.
// Normals are axis-aligned after compute_vertex_normals (decision D1).
Mesh make_cube(Vec3 size);

// Tight axis-aligned bounding box around all positions.
// Returns {{0,0,0},{0,0,0}} for an empty mesh.
Aabb mesh_bounds(const Mesh& m);

} // namespace openphysx
