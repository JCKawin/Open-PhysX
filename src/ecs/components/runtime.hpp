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

// Regenerable solver output. It belongs in the sidecar cache, never in the project file.
struct CfdResultField
{
    int cells = 0;
    float time = 0.0f;
};

inline bool operator==(const SelectionOutlineTag& a, const SelectionOutlineTag& b)
{
    return a.marked == b.marked;
}

inline bool operator==(const MeshGpuHandle& a, const MeshGpuHandle& b)
{
    return a.vao == b.vao && a.vbo == b.vbo;
}

inline bool operator==(const PhysicsBodyHandle& a, const PhysicsBodyHandle& b)
{
    return a.index == b.index;
}

inline bool operator==(const ContactCache& a, const ContactCache& b)
{
    return a.count == b.count;
}

inline bool operator==(const CfdResultField& a, const CfdResultField& b)
{
    return a.cells == b.cells && a.time == b.time;
}

} // namespace openphysx
