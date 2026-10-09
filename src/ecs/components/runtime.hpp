#pragma once

#include <cstdint>

namespace openphysx {

// Runtime only. Never written to the project file.
// A bool keeps the type non-empty: EnTT 3.15 does not return a reference for empty components.
struct SelectionOutlineTag
{
    bool marked = true;
};

struct MeshGpuHandle
{
    std::uint32_t vao = 0;
    std::uint32_t vbo = 0;
};

struct PhysicsBodyHandle
{
    int index = -1;
};

struct ContactCache
{
    int count = 0;
};

} // namespace openphysx
