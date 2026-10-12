// B3.3 + B3.4 — STL importer (binary and ASCII).
// Layering rule: no raylib, no ImGui.
// Detection rule: binary iff file_size == 84 + 50 * triangle_count.
// A binary file may start with "solid" — only the size test decides.
// ASCII: tokenise on whitespace; "vertex x y z" pushes a position; three per triangle.

#include "io/StlImport.h"

#include "core/Mesh.h"
#include "io/Import.h"
#include "io/Weld.h"

#include <bit>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace openphysx {
namespace {

// Read the whole file as raw bytes.
std::vector<uint8_t> read_file_bytes(const std::filesystem::path& path, std::string& err)
{
    FILE* f = nullptr;
#if defined(_WIN32)
    f = _wfopen(path.c_str(), L"rb");
#else
    f = std::fopen(path.c_str(), "rb");
#endif
    if (!f) { err = "Cannot open file: " + path.string(); return {}; }
    std::fseek(f, 0, SEEK_END);
    const long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(static_cast<std::size_t>(sz));
    std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    return buf;
}

// Read a little-endian u32 from raw bytes.
uint32_t read_u32_le(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

// Read a little-endian IEEE-754 float.
float read_f32_le(const uint8_t* p)
{
    uint32_t bits = read_u32_le(p);
    return std::bit_cast<float>(bits);
}

// True if the file matches binary STL size constraints.
bool looks_binary(std::size_t file_size, const uint8_t* bytes)
{
    if (file_size < 84) return false;
    const uint32_t count = read_u32_le(bytes + 80);
    return file_size == 84 + static_cast<uint64_t>(count) * 50;
}

// Apply opts (z_up conversion + scale) to a triangle (3 positions) and push into mesh.
void push_triangle(Mesh& m, Vec3 a, Vec3 b, Vec3 c, const ImportOptions& opts)
{
    auto xform = [&](Vec3 p) -> Vec3 {
        if (opts.z_up) p = z_up_to_y_up(p);
        p.x *= opts.scale; p.y *= opts.scale; p.z *= opts.scale;
        return p;
    };
    const uint32_t base = static_cast<uint32_t>(m.positions.size());
    m.positions.push_back(xform(a));
    m.positions.push_back(xform(b));
    m.positions.push_back(xform(c));
    m.indices.push_back(base + 0);
    m.indices.push_back(base + 1);
    m.indices.push_back(base + 2);
}

ImportResult import_stl_binary(const std::vector<uint8_t>& bytes, const ImportOptions& opts)
{
    ImportResult r;
    const uint32_t count = read_u32_le(bytes.data() + 80);
    Mesh out;
    out.positions.reserve(count * 3);
    out.indices.reserve(count * 3);

    for (uint32_t i = 0; i < count; ++i)
    {
        // Each facet: 12 bytes normal (ignored) + 3×12 bytes vertices + 2 attribute bytes
        const uint8_t* facet = bytes.data() + 84 + i * 50;
        // Skip normal (12 bytes), read 3 vertices
        const Vec3 va{ read_f32_le(facet + 12), read_f32_le(facet + 16), read_f32_le(facet + 20) };
        const Vec3 vb{ read_f32_le(facet + 24), read_f32_le(facet + 28), read_f32_le(facet + 32) };
        const Vec3 vc{ read_f32_le(facet + 36), read_f32_le(facet + 40), read_f32_le(facet + 44) };
        push_triangle(out, va, vb, vc, opts);
    }

    if (out.indices.empty())
    {
        r.error = "STL file has no triangles";
        return r;
    }
    weld_by_position(out, 1e-6f);
    compute_vertex_normals(out);
    r.mesh = std::move(out);
    return r;
}

bool parse_float_sv(const char* begin, const char* end, float& out)
{
    auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{};
}

ImportResult import_stl_ascii(const std::vector<uint8_t>& bytes, const ImportOptions& opts)
{
    ImportResult r;
    // Convert to string_view for scanning
    const char* data = reinterpret_cast<const char*>(bytes.data());
    const std::size_t size = bytes.size();

    Mesh out;
    std::vector<Vec3> face_verts; // collects 3 vertices per face
    face_verts.reserve(3);

    std::size_t pos = 0;
    auto skip_ws = [&]() {
        while (pos < size && (data[pos] == ' ' || data[pos] == '\t' ||
                              data[pos] == '\n' || data[pos] == '\r'))
            ++pos;
    };
    auto read_token = [&](std::string& tok) -> bool {
        skip_ws();
        if (pos >= size) return false;
        std::size_t start = pos;
        while (pos < size && data[pos] != ' ' && data[pos] != '\t' &&
               data[pos] != '\n' && data[pos] != '\r')
            ++pos;
        tok.assign(data + start, data + pos);
        return !tok.empty();
    };

    std::string tok;
    while (read_token(tok))
    {
        if (tok == "vertex")
        {
            // vertex x y z
            std::string sx, sy, sz;
            if (!read_token(sx) || !read_token(sy) || !read_token(sz))
            {
                r.error = "STL ASCII: malformed 'vertex' line";
                return r;
            }
            Vec3 v{};
            if (!parse_float_sv(sx.data(), sx.data() + sx.size(), v.x) ||
                !parse_float_sv(sy.data(), sy.data() + sy.size(), v.y) ||
                !parse_float_sv(sz.data(), sz.data() + sz.size(), v.z))
            {
                r.error = "STL ASCII: vertex coordinate is not a number";
                return r;
            }
            face_verts.push_back(v);
            if (face_verts.size() == 3)
            {
                push_triangle(out, face_verts[0], face_verts[1], face_verts[2], opts);
                face_verts.clear();
            }
        }
        // "solid", "facet", "normal", "outer", "loop", "endloop", "endfacet", "endsolid" — ignored
    }

    if (out.indices.empty())
    {
        r.error = "STL ASCII: no triangles found";
        return r;
    }
    weld_by_position(out, 1e-6f);
    compute_vertex_normals(out);
    r.mesh = std::move(out);
    return r;
}

} // namespace

ImportResult import_stl(const std::filesystem::path& path, const ImportOptions& opts)
{
    ImportResult r;
    const std::vector<uint8_t> bytes = read_file_bytes(path, r.error);
    if (!r.error.empty()) return r;

    if (looks_binary(bytes.size(), bytes.data()))
        return import_stl_binary(bytes, opts);
    else
        return import_stl_ascii(bytes, opts);
}

} // namespace openphysx
