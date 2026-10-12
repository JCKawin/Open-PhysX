// T-B1-01 … T-B1-05 — Mesh component tests (doctest, no raylib).

#include "core/Mesh.h"
#include "core/Math.h"

#include <doctest/doctest.h>
#include <cmath>
#include <string>

using namespace openphysx;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static float vec_len(Vec3 v)
{
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

static float dot3(Vec3 a, Vec3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

// ---------------------------------------------------------------------------
// T-B1-01  validate() reports errors correctly
// ---------------------------------------------------------------------------
TEST_CASE("T-B1-01: Mesh::validate detects structural errors")
{
    // Valid empty mesh
    Mesh empty;
    CHECK(empty.validate().empty());

    // Indices not multiple of 3
    Mesh m1;
    m1.positions = {{0,0,0},{1,0,0}};
    m1.indices = {0, 1};
    CHECK_FALSE(m1.validate().empty());

    // Index out of range
    Mesh m2;
    m2.positions = {{0,0,0},{1,0,0},{0,1,0}};
    m2.indices = {0, 1, 5}; // 5 is out of range
    CHECK_FALSE(m2.validate().empty());

    // Normal count mismatch
    Mesh m3;
    m3.positions = {{0,0,0},{1,0,0},{0,1,0}};
    m3.indices = {0, 1, 2};
    m3.normals = {{0,1,0}}; // 1 normal but 3 positions
    CHECK_FALSE(m3.validate().empty());

    // Valid: empty normals allowed (pre-compute state)
    Mesh m4;
    m4.positions = {{0,0,0},{1,0,0},{0,1,0}};
    m4.indices = {0, 1, 2};
    CHECK(m4.validate().empty());
}

// ---------------------------------------------------------------------------
// T-B1-02  compute_vertex_normals produces unit vectors
// ---------------------------------------------------------------------------
TEST_CASE("T-B1-02: compute_vertex_normals outputs unit normals")
{
    Mesh m;
    m.positions = {{0,0,0},{1,0,0},{0,1,0}};
    m.indices = {0, 1, 2};
    compute_vertex_normals(m);
    REQUIRE(m.normals.size() == 3);
    for (const Vec3& n : m.normals)
    {
        const float l = vec_len(n);
        CHECK(std::fabs(l - 1.0f) < 1e-5f);
    }
}

// ---------------------------------------------------------------------------
// T-B1-03  Zero-area triangle does not produce NaN
// ---------------------------------------------------------------------------
TEST_CASE("T-B1-03: zero-area face does not produce NaN normals")
{
    Mesh m;
    m.positions = {{0,0,0},{0,0,0},{0,0,0}}; // degenerate
    m.indices = {0, 1, 2};
    compute_vertex_normals(m);
    REQUIRE(m.normals.size() == 3);
    for (const Vec3& n : m.normals)
    {
        CHECK_FALSE(std::isnan(n.x));
        CHECK_FALSE(std::isnan(n.y));
        CHECK_FALSE(std::isnan(n.z));
    }
}

// ---------------------------------------------------------------------------
// T-B1-04  make_cube: all face normals point away from centre
// ---------------------------------------------------------------------------
TEST_CASE("T-B1-04: make_cube normals point outward")
{
    const Vec3 size{2.0f, 2.0f, 2.0f};
    const Mesh m = make_cube(size);
    CHECK(m.validate().empty());
    CHECK(m.positions.size() == 24);
    CHECK(m.indices.size() == 36);

    const Vec3 center{0,0,0};
    const std::size_t tri_count = m.indices.size() / 3;
    for (std::size_t t = 0; t < tri_count; ++t)
    {
        const uint32_t ia = m.indices[t*3+0];
        const uint32_t ib = m.indices[t*3+1];
        const uint32_t ic = m.indices[t*3+2];

        // Geometric normal of this triangle
        Vec3 ab = vec_sub(m.positions[ib], m.positions[ia]);
        Vec3 ac = vec_sub(m.positions[ic], m.positions[ia]);
        Vec3 gn = vec_normalize(vec_cross(ab, ac));

        // Centroid
        Vec3 cen{
            (m.positions[ia].x + m.positions[ib].x + m.positions[ic].x) / 3.0f,
            (m.positions[ia].y + m.positions[ib].y + m.positions[ic].y) / 3.0f,
            (m.positions[ia].z + m.positions[ib].z + m.positions[ic].z) / 3.0f,
        };
        Vec3 to_center = vec_sub(center, cen);

        // Geometric normal should point AWAY from center => dot < 0 with to_center
        CHECK(dot3(gn, to_center) < 0.0f);
    }

    // Every stored vertex normal should also point away from center
    for (std::size_t i = 0; i < m.positions.size(); ++i)
    {
        Vec3 to_center = vec_sub(center, m.positions[i]);
        CHECK(dot3(m.normals[i], to_center) < 0.0f);
    }
}

// ---------------------------------------------------------------------------
// T-B1-05  make_cube: bounds equal the requested size
// ---------------------------------------------------------------------------
TEST_CASE("T-B1-05: make_cube bounds equal size")
{
    const Vec3 size{4.0f, 2.0f, 6.0f};
    const Mesh m = make_cube(size);
    const Aabb box = mesh_bounds(m);
    CHECK(std::fabs(box.max.x - box.min.x - size.x) < 1e-5f);
    CHECK(std::fabs(box.max.y - box.min.y - size.y) < 1e-5f);
    CHECK(std::fabs(box.max.z - box.min.z - size.z) < 1e-5f);
    CHECK(std::fabs((box.min.x + box.max.x) * 0.5f) < 1e-5f); // centred
    CHECK(std::fabs((box.min.y + box.max.y) * 0.5f) < 1e-5f);
    CHECK(std::fabs((box.min.z + box.max.z) * 0.5f) < 1e-5f);
}
