// B3.10 — Format detection implementation.

#include "io/FormatDetect.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace openphysx {
namespace {

// Lower-case an ASCII string (extensions only — no locale, no Unicode).
std::string ascii_lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool starts_with_bytes(std::span<const uint8_t> head, const char* magic, std::size_t n)
{
    if (head.size() < n) return false;
    return std::memcmp(head.data(), magic, n) == 0;
}

} // namespace

Format detect_format(const std::filesystem::path& path,
                     std::span<const uint8_t>      head)
{
    // --- Magic-byte checks (higher priority than extension) ---

    // Uncompressed .blend: starts with "BLENDER"
    if (starts_with_bytes(head, "BLENDER", 7))
        return Format::Blend;

    // GLB: starts with 0x46546C67 little-endian = "glTF"
    if (starts_with_bytes(head, "glTF", 4))
        return Format::Glb;

    // --- Extension check ---
    const std::string ext = ascii_lower(path.extension().string());

    // .blend and .blend1 — also catches gzip/zstd-compressed .blend files
    // (the magic bytes differ, but the extension already tells us it is a .blend)
    if (ext == ".blend" || ext == ".blend1")
        return Format::Blend;

    if (ext == ".obj") return Format::Obj;
    if (ext == ".stl") return Format::Stl;
    if (ext == ".ply") return Format::Ply;
    if (ext == ".glb") return Format::Glb;
    if (ext == ".fbx") return Format::Fbx;
    if (ext == ".abc") return Format::Abc;

    return Format::Unknown;
}

} // namespace openphysx
