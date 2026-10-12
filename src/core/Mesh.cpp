// B1.1, B1.2, B1.3 — Mesh implementation.
// No raylib or ImGui. No iostream (locale-independence rule: only from_chars / to_chars for I/O).

#include "core/Mesh.h"
#include "core/Math.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>

namespace openphysx {

// ---------------------------------------------------------------------------
// B1.1  validate()
// ---------------------------------------------------------------------------
std::string Mesh::validate() const
{
    if (indices.size() % 3 != 0)
        return "index count is not a multiple of 3";

    for (uint32_t i : indices)
    {
        if (i >= positions.size())
            return "index out of range";
    }

    if (!normals.empty() && normals.size() != positions.size())
        return "normals.size() != positions.size()";

    return {};
}

// ---------------------------------------------------------------------------
// B1.2  compute_vertex_normals
// Area-weighted accumulation (cross product length = 2 * area); normalised at
// the end. Zero-length normals fall back to (0,1,0) to avoid NaN.
// Called ONCE after mesh creation/import. Never call from the renderer.
// ---------------------------------------------------------------------------
void compute_vertex_normals(Mesh& m)
{
    m.normals.assign(m.positions.size(), Vec3{0.0f, 0.0f, 0.0f});

    const std::size_t tri_count = m.indices.size() / 3;
    for (std::size_t t = 0; t < tri_count; ++t)
    {
        const uint32_t ia = m.indices[t * 3 + 0];
        const uint32_t ib = m.indices[t * 3 + 1];
        const uint32_t ic = m.indices[t * 3 + 2];

        const Vec3& a = m.positions[ia];
        const Vec3& b = m.positions[ib];
        const Vec3& c = m.positions[ic];

        // cross(b-a, c-a) — length equals 2*face_area; acts as area weight
        const Vec3 n = vec_cross(vec_sub(b, a), vec_sub(c, a));

        // Accumulate into all three corners
        Vec3& na = m.normals[ia];
        na.x += n.x; na.y += n.y; na.z += n.z;
        Vec3& nb = m.normals[ib];
        nb.x += n.x; nb.y += n.y; nb.z += n.z;
        Vec3& nc = m.normals[ic];
        nc.x += n.x; nc.y += n.y; nc.z += n.z;
    }

    for (Vec3& n : m.normals)
    {
        const float l = vec_length(n);
        if (l > 1.0e-20f)
        {
            n.x /= l; n.y /= l; n.z /= l;
        }
        else
        {
            // Degenerate or isolated vertex — safe fallback
            n = Vec3{0.0f, 1.0f, 0.0f};
        }
    }
}

// ---------------------------------------------------------------------------
// B1.3  make_cube  (decision D1: 24 vertices, 4 per face, never shared)
//
// For each face: outward normal n, two tangents u and v such that u × v = n.
// Four corners (CCW from outside): -u-v, +u-v, +u+v, -u+v.
// Two triangles: (0,1,2) and (0,2,3).
//
// Face table (n, u, v):
//   +X: (1,0,0)  (0,1,0)  (0,0,1)
//   -X: (-1,0,0) (0,0,1)  (0,1,0)
//   +Y: (0,1,0)  (0,0,1)  (1,0,0)
//   -Y: (0,-1,0) (1,0,0)  (0,0,1)
//   +Z: (0,0,1)  (1,0,0)  (0,1,0)
//   -Z: (0,0,-1) (0,1,0)  (1,0,0)
// ---------------------------------------------------------------------------
Mesh make_cube(Vec3 size)
{
    struct FaceDef { Vec3 n, u, v; };
    static constexpr FaceDef kFaces[6] = {
        {{ 1, 0, 0}, {0, 1, 0}, {0, 0, 1}},
        {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
        {{ 0, 1, 0}, {0, 0, 1}, {1, 0, 0}},
        {{ 0,-1, 0}, {1, 0, 0}, {0, 0, 1}},
        {{ 0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
        {{ 0, 0,-1}, {0, 1, 0}, {1, 0, 0}},
    };

    const float hx = size.x * 0.5f;
    const float hy = size.y * 0.5f;
    const float hz = size.z * 0.5f;

    Mesh m;
    m.positions.reserve(24);
    m.indices.reserve(36);

    for (int f = 0; f < 6; ++f)
    {
        const Vec3& fn = kFaces[f].n;
        const Vec3& fu = kFaces[f].u;
        const Vec3& fv = kFaces[f].v;

        // Half-extents along each axis for this face's normal direction
        const float hn = std::fabs(fn.x) * hx + std::fabs(fn.y) * hy + std::fabs(fn.z) * hz;
        const float hu = std::fabs(fu.x) * hx + std::fabs(fu.y) * hy + std::fabs(fu.z) * hz;
        const float hv = std::fabs(fv.x) * hx + std::fabs(fv.y) * hy + std::fabs(fv.z) * hz;

        // Four CCW corners (seen from outside): -u-v, +u-v, +u+v, -u+v
        const uint32_t base = static_cast<uint32_t>(m.positions.size());
        auto add = [&](float su, float sv) {
            m.positions.push_back({
                fn.x * hn + fu.x * su * hu + fv.x * sv * hv,
                fn.y * hn + fu.y * su * hu + fv.y * sv * hv,
                fn.z * hn + fu.z * su * hu + fv.z * sv * hv,
            });
        };
        add(-1, -1); // 0
        add(+1, -1); // 1
        add(+1, +1); // 2
        add(-1, +1); // 3

        // Two CCW triangles
        m.indices.push_back(base + 0);
        m.indices.push_back(base + 1);
        m.indices.push_back(base + 2);
        m.indices.push_back(base + 0);
        m.indices.push_back(base + 2);
        m.indices.push_back(base + 3);
    }

    compute_vertex_normals(m);
    return m;
}

// ---------------------------------------------------------------------------
// mesh_bounds
// ---------------------------------------------------------------------------
Aabb mesh_bounds(const Mesh& m)
{
    if (m.positions.empty())
        return {};

    Aabb box;
    box.min = box.max = m.positions[0];
    for (const Vec3& p : m.positions)
    {
        if (p.x < box.min.x) box.min.x = p.x;
        if (p.y < box.min.y) box.min.y = p.y;
        if (p.z < box.min.z) box.min.z = p.z;
        if (p.x > box.max.x) box.max.x = p.x;
        if (p.y > box.max.y) box.max.y = p.y;
        if (p.z > box.max.z) box.max.z = p.z;
    }
    return box;
}

} // namespace openphysx
