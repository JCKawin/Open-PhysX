// B3.2 — OBJ importer.
// Reads v (positions) and f (faces), fan-triangulates, handles negative indices,
// ignores vt/vn/o/g/s/usemtl/mtllib and comments.
// All float parsing uses std::from_chars (locale-independent).
// Layering rule: no raylib, no ImGui.

#include "io/ObjImport.h"

#include "core/Mesh.h"
#include "io/Import.h"
#include "io/Weld.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace openphysx {
namespace {

// Read the whole file into a string.
std::string read_text_file(const std::filesystem::path& path, std::string& err)
{
    // Open in binary mode so \r\n works on all platforms.
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
    std::string out(static_cast<std::size_t>(sz), '\0');
    std::fread(out.data(), 1, static_cast<std::size_t>(sz), f);
    std::fclose(f);
    return out;
}

// Parse float with from_chars (locale-independent).
// Returns false on failure.
bool parse_float(std::string_view sv, float& out)
{
    // Skip leading whitespace
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
        sv.remove_prefix(1);
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
    return ec == std::errc{};
}

// Parse int (1-based OBJ index) with from_chars.
bool parse_int(std::string_view sv, int32_t& out)
{
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
        sv.remove_prefix(1);
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
    return ec == std::errc{};
}

// Split a string_view on whitespace, returning tokens (no empty tokens).
std::vector<std::string_view> split_ws(std::string_view line)
{
    std::vector<std::string_view> tokens;
    std::size_t i = 0;
    while (i < line.size())
    {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size()) break;
        std::size_t j = i;
        while (j < line.size() && line[j] != ' ' && line[j] != '\t') ++j;
        tokens.push_back(line.substr(i, j - i));
        i = j;
    }
    return tokens;
}

// From a face corner token like "1", "1/2", "1//3", "1/2/3",
// extract just the vertex index (before the first '/').
std::string_view vertex_part(std::string_view tok)
{
    const std::size_t slash = tok.find('/');
    return (slash == std::string_view::npos) ? tok : tok.substr(0, slash);
}

} // namespace

ImportResult import_obj(const std::filesystem::path& path, const ImportOptions& opts)
{
    ImportResult r;
    std::string text = read_text_file(path, r.error);
    if (!r.error.empty()) return r;

    std::vector<Vec3> raw_positions;
    Mesh out;
    int  line_no = 0;

    std::string_view src(text);
    while (!src.empty())
    {
        // Extract one line
        std::size_t nl = src.find('\n');
        std::string_view line = (nl == std::string_view::npos) ? src : src.substr(0, nl);
        src = (nl == std::string_view::npos) ? std::string_view{} : src.substr(nl + 1);
        ++line_no;

        // Strip \r
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        // Strip comment
        const std::size_t hash = line.find('#');
        if (hash != std::string_view::npos)
            line = line.substr(0, hash);

        const auto tokens = split_ws(line);
        if (tokens.empty()) continue;

        const std::string_view cmd = tokens[0];

        if (cmd == "v")
        {
            // v x y z [w] [r g b]
            if (tokens.size() < 4)
            {
                r.error = "Line " + std::to_string(line_no) + ": 'v' needs x y z";
                return r;
            }
            Vec3 p{};
            if (!parse_float(tokens[1], p.x) || !parse_float(tokens[2], p.y) || !parse_float(tokens[3], p.z))
            {
                r.error = "Line " + std::to_string(line_no) + ": malformed vertex coordinates";
                return r;
            }
            // Apply axis conversion and scale
            if (opts.z_up)
                p = z_up_to_y_up(p);
            p.x *= opts.scale; p.y *= opts.scale; p.z *= opts.scale;
            raw_positions.push_back(p);
        }
        else if (cmd == "f")
        {
            // f c0 c1 c2 [c3 ...]  — fan triangulate
            if (tokens.size() < 4)
            {
                r.error = "Line " + std::to_string(line_no) + ": face needs at least 3 vertices";
                return r;
            }
            // Resolve indices for all corners
            std::vector<uint32_t> corners;
            corners.reserve(tokens.size() - 1);
            for (std::size_t i = 1; i < tokens.size(); ++i)
            {
                int32_t idx;
                if (!parse_int(vertex_part(tokens[i]), idx))
                {
                    r.error = "Line " + std::to_string(line_no) + ": invalid face index";
                    return r;
                }
                // OBJ: 1-based positive; negative = from end at this moment
                int64_t resolved;
                if (idx > 0)
                    resolved = static_cast<int64_t>(idx) - 1;
                else if (idx < 0)
                    resolved = static_cast<int64_t>(raw_positions.size()) + static_cast<int64_t>(idx);
                else
                {
                    r.error = "Line " + std::to_string(line_no) + ": face index 0 is invalid in OBJ";
                    return r;
                }
                if (resolved < 0 || static_cast<std::size_t>(resolved) >= raw_positions.size())
                {
                    r.error = "Line " + std::to_string(line_no) + ": face index out of range";
                    return r;
                }
                corners.push_back(static_cast<uint32_t>(resolved));
            }
            // Fan triangulation: (c0, c1, c2), (c0, c2, c3), ...
            for (std::size_t i = 1; i + 1 < corners.size(); ++i)
            {
                out.indices.push_back(corners[0]);
                out.indices.push_back(corners[i]);
                out.indices.push_back(corners[i + 1]);
            }
        }
        // else: vt, vn, o, g, s, usemtl, mtllib — silently ignored
    }

    if (raw_positions.empty())
    {
        r.error = "OBJ file has no vertices";
        return r;
    }
    if (out.indices.empty())
    {
        r.error = "OBJ file has no faces";
        return r;
    }

    // Positions were already stored in raw_positions; copy them into the mesh.
    out.positions = std::move(raw_positions);

    compute_vertex_normals(out);
    r.mesh = std::move(out);
    return r;
}

} // namespace openphysx
