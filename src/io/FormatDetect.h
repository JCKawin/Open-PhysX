#pragma once

// B3.10 — Format detection and .blend rejection.
// Layering rule: no raylib, no ImGui.
// detect_format() checks the file extension and the first bytes (magic).

#include <cstring>
#include <filesystem>
#include <span>
#include <string>

namespace openphysx {

enum class Format
{
    Obj,
    Stl,
    Ply,
    Glb,
    Fbx,
    Abc,
    Blend,   // .blend / BLENDER magic — always rejected with a message
    Unknown, // unsupported / unrecognised extension
};

// Detect the file format from extension and the first 16 bytes of the file.
// head may be shorter than 16 bytes if the file is that small.
// Priority: magic bytes first (BLENDER, GLB), then extension.
Format detect_format(const std::filesystem::path& path,
                     std::span<const uint8_t>      head /*first ≤ 16 bytes*/);

// Human-readable message shown when the user tries to open/import a .blend.
inline const char* blend_rejection_message()
{
    return "Blender .blend files are not supported. "
           "In Blender use File > Export (glTF .glb, FBX, OBJ, STL or Alembic), "
           "then import that file.";
}

} // namespace openphysx
