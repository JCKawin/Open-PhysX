// T-B2-01 / T-B2-02 — Binary helpers tests.
// T-B3-01 / T-B3-02 — Axis conversion tests.
// T-B3-15 / T-B3-16 — Format detection and .blend rejection.

#include "io/Binary.h"
#include "io/FormatDetect.h"
#include "io/Import.h"

#include <doctest/doctest.h>
#include <cmath>
#include <cstring>

using namespace openphysx;

// ---------------------------------------------------------------------------
// T-B2-01  Writer produces correct LE bytes
// ---------------------------------------------------------------------------
TEST_CASE("T-B2-01: Writer writes correct little-endian bytes")
{
    Writer w;
    w.u8(0xAB);
    w.u32(0x12345678);
    w.u64(0x0102030405060708ULL);
    w.f32(1.0f);
    w.str("hi");

    REQUIRE(w.buf.size() == 1 + 4 + 8 + 4 + 4 + 2);
    // u8
    CHECK(w.buf[0] == 0xAB);
    // u32 LE
    CHECK(w.buf[1] == 0x78);
    CHECK(w.buf[2] == 0x56);
    CHECK(w.buf[3] == 0x34);
    CHECK(w.buf[4] == 0x12);
    // u64 LE — first byte is 0x08
    CHECK(w.buf[5] == 0x08);
    // f32 1.0f = 0x3F800000
    CHECK(w.buf[13] == 0x00);
    CHECK(w.buf[14] == 0x00);
    CHECK(w.buf[15] == 0x80);
    CHECK(w.buf[16] == 0x3F);
    // str: u32 length = 2, then 'h' 'i'
    CHECK(w.buf[17] == 0x02);
    CHECK(w.buf[18] == 0x00);
    CHECK(w.buf[19] == 0x00);
    CHECK(w.buf[20] == 0x00);
    CHECK(w.buf[21] == 'h');
    CHECK(w.buf[22] == 'i');
}

// ---------------------------------------------------------------------------
// T-B2-02  Reader round-trips what Writer produces; overrun returns false
// ---------------------------------------------------------------------------
TEST_CASE("T-B2-02: Reader round-trips Writer output and rejects overrun")
{
    Writer w;
    w.u8(7);
    w.u32(12345);
    w.u64(987654321ULL);
    w.f32(3.14f);
    w.str("hello");

    Reader r(w.buf.data(), w.buf.size());

    uint8_t  u8v;  CHECK(r.u8(u8v));   CHECK(u8v == 7);
    uint32_t u32v; CHECK(r.u32(u32v)); CHECK(u32v == 12345);
    uint64_t u64v; CHECK(r.u64(u64v)); CHECK(u64v == 987654321ULL);
    float    fv;   CHECK(r.f32(fv));   CHECK(std::fabs(fv - 3.14f) < 1e-5f);
    std::string sv; CHECK(r.str(sv));  CHECK(sv == "hello");

    // Now exhausted — next read must fail
    uint8_t extra;
    CHECK_FALSE(r.u8(extra));
}

TEST_CASE("T-B2-02b: Reader rejects string longer than remaining bytes")
{
    // Write a length prefix that says 100 bytes but only 3 follow
    Writer w;
    w.u32(100);
    w.u8('a'); w.u8('b'); w.u8('c');
    Reader r(w.buf.data(), w.buf.size());
    std::string s;
    CHECK_FALSE(r.str(s));
}

// ---------------------------------------------------------------------------
// T-B3-01  z_up_to_y_up: (0,0,1) → (0,1,0)
// ---------------------------------------------------------------------------
TEST_CASE("T-B3-01: z_up_to_y_up converts (0,0,1) to (0,1,0)")
{
    const Vec3 in{0.0f, 0.0f, 1.0f};
    const Vec3 out = z_up_to_y_up(in);
    CHECK(std::fabs(out.x) < 1e-6f);
    CHECK(std::fabs(out.y - 1.0f) < 1e-6f);
    CHECK(std::fabs(out.z) < 1e-6f);
}

// ---------------------------------------------------------------------------
// T-B3-02  z_up_to_y_up: determinant +1, winding preserved
// ---------------------------------------------------------------------------
TEST_CASE("T-B3-02: z_up_to_y_up has determinant +1")
{
    // The matrix is [1 0 0; 0 0 1; 0 -1 0].
    // Determinant = 1*(0*0 - 1*(-1)) - 0 + 0 = 1. Check via triple product.
    // If we map the canonical basis and compute det: should be +1.
    const Vec3 ex = z_up_to_y_up({1,0,0}); // → (1,0,0)
    const Vec3 ey = z_up_to_y_up({0,1,0}); // → (0,0,-1)
    const Vec3 ez = z_up_to_y_up({0,0,1}); // → (0,1,0)

    // scalar triple product ex · (ey × ez) = det
    const Vec3 cross{
        ey.y * ez.z - ey.z * ez.y,
        ey.z * ez.x - ey.x * ez.z,
        ey.x * ez.y - ey.y * ez.x,
    };
    const float det = ex.x * cross.x + ex.y * cross.y + ex.z * cross.z;
    CHECK(std::fabs(det - 1.0f) < 1e-5f);
}

// ---------------------------------------------------------------------------
// T-B3-15  detect_format: correct classification
// ---------------------------------------------------------------------------
TEST_CASE("T-B3-15: detect_format classifies by extension and magic")
{
    const uint8_t empty_head[16] = {};
    auto fmt = [&](const char* filename) {
        return detect_format(std::filesystem::path(filename),
                             std::span<const uint8_t>(empty_head, 16));
    };

    CHECK(fmt("model.obj") == Format::Obj);
    CHECK(fmt("model.OBJ") == Format::Obj);
    CHECK(fmt("model.stl") == Format::Stl);
    CHECK(fmt("model.ply") == Format::Ply);
    CHECK(fmt("model.fbx") == Format::Fbx);
    CHECK(fmt("model.abc") == Format::Abc);
    CHECK(fmt("model.xyz") == Format::Unknown);

    // .blend extension always → Blend regardless of magic
    CHECK(fmt("scene.blend") == Format::Blend);
    CHECK(fmt("scene.blend1") == Format::Blend);
}

TEST_CASE("T-B3-16: detect_format catches BLENDER magic bytes")
{
    // Magic: "BLENDER-v302" (first 7 bytes are "BLENDER")
    const char raw[] = "BLENDER-v302";
    std::span<const uint8_t> head(reinterpret_cast<const uint8_t*>(raw), std::strlen(raw));

    // Even if the extension is .obj — magic wins
    CHECK(detect_format(std::filesystem::path("trick.obj"), head) == Format::Blend);
}

TEST_CASE("T-B3-16b: detect_format catches GLB magic bytes")
{
    const char raw[] = "glTF\x02\x00\x00\x00";
    std::span<const uint8_t> head(reinterpret_cast<const uint8_t*>(raw), 8);
    CHECK(detect_format(std::filesystem::path("file.bin"), head) == Format::Glb);
    CHECK(detect_format(std::filesystem::path("file.glb"), head) == Format::Glb);
}

// ---------------------------------------------------------------------------
// T-B3-03..06: OBJ Importer tests
// ---------------------------------------------------------------------------
#include "io/ObjImport.h"
#include "io/ObjExport.h"
#include "io/StlImport.h"
#include <fstream>

TEST_CASE("T-B3-03: OBJ import reads vertices, faces, fan triangulation, negative indices")
{
    const std::filesystem::path temp_obj = std::filesystem::temp_directory_path() / "test_quad.obj";
    {
        std::ofstream ofs(temp_obj);
        ofs << "# Comment line\n";
        ofs << "v 0.0 0.0 0.0\n";
        ofs << "v 1.0 0.0 0.0\n";
        ofs << "v 1.0 1.0 0.0\n";
        ofs << "v 0.0 1.0 0.0\n";
        // Quad face with negative indices relative to 4 vertices
        ofs << "f -4 -3 -2 -1\n";
    }

    ImportOptions opts{};
    opts.z_up = false;
    opts.scale = 1.0f;
    const ImportResult r = import_obj(temp_obj, opts);
    std::filesystem::remove(temp_obj);

    REQUIRE(r.mesh.has_value());
    CHECK(r.error.empty());
    CHECK(r.mesh->positions.size() == 4);
    // Quad fan-triangulated into 2 triangles (6 indices)
    CHECK(r.mesh->indices.size() == 6);
    CHECK(r.mesh->indices[0] == 0);
    CHECK(r.mesh->indices[1] == 1);
    CHECK(r.mesh->indices[2] == 2);
    CHECK(r.mesh->indices[3] == 0);
    CHECK(r.mesh->indices[4] == 2);
    CHECK(r.mesh->indices[5] == 3);
}

TEST_CASE("T-B4-09: OBJ export and re-import round trip")
{
    const Mesh cube = make_cube({2.0f, 2.0f, 2.0f});
    const std::filesystem::path temp_obj = std::filesystem::temp_directory_path() / "test_cube_roundtrip.obj";

    ObjExportOptions export_opts{};
    export_opts.apply_transform = false;
    std::string err = export_obj(cube, "TestCube", nullptr, export_opts, temp_obj);
    REQUIRE(err.empty());

    ImportOptions import_opts{};
    import_opts.z_up = false;
    import_opts.scale = 1.0f;
    const ImportResult r = import_obj(temp_obj, import_opts);
    std::filesystem::remove(temp_obj);

    REQUIRE(r.mesh.has_value());
    CHECK(r.mesh->positions.size() == cube.positions.size());
    CHECK(r.mesh->indices.size() == cube.indices.size());
}
