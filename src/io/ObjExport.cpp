// B4.4 — OBJ exporter.
// Writes o <name>, v x y z, f a b c (1-based indices).
// Uses to_chars (locale-independent). No vn.
// Writes the original mesh positions — never an evaluated copy.

#include "io/ObjExport.h"

#include <charconv>
#include <cstdio>
#include <string>

namespace openphysx {
namespace {

// Format a float to shortest round-trip representation via to_chars.
std::string float_str(float v)
{
    char buf[32];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
    if (ec != std::errc{}) return "0";
    return std::string(buf, ptr);
}

} // namespace

std::string export_obj(const Mesh& mesh,
                       const std::string& name,
                       const float* world,
                       const ObjExportOptions& opts,
                       const std::filesystem::path& path)
{
    FILE* f = nullptr;
#if defined(_WIN32)
    f = _wfopen(path.c_str(), L"wb");
#else
    f = std::fopen(path.c_str(), "wb");
#endif
    if (!f) return "Cannot open file for writing: " + path.string();

    // Object name
    std::fprintf(f, "o %s\n", name.c_str());

    // Vertices
    for (const Vec3& p : mesh.positions)
    {
        Vec3 out = p;
        if (opts.apply_transform && world)
        {
            // Column-major matrix multiply: out = M * p (homogeneous, w=1)
            const float* m = world;
            out.x = m[0]*p.x + m[4]*p.y + m[8]*p.z  + m[12];
            out.y = m[1]*p.x + m[5]*p.y + m[9]*p.z  + m[13];
            out.z = m[2]*p.x + m[6]*p.y + m[10]*p.z + m[14];
        }
        std::fprintf(f, "v %s %s %s\n",
            float_str(out.x).c_str(),
            float_str(out.y).c_str(),
            float_str(out.z).c_str());
    }

    // Faces (1-based)
    const std::size_t tri_count = mesh.indices.size() / 3;
    for (std::size_t t = 0; t < tri_count; ++t)
    {
        std::fprintf(f, "f %u %u %u\n",
            mesh.indices[t * 3 + 0] + 1,
            mesh.indices[t * 3 + 1] + 1,
            mesh.indices[t * 3 + 2] + 1);
    }

    std::fclose(f);
    return {};
}

} // namespace openphysx
