#pragma once

#include "core/Types.h"

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
    int built_shape = -1;
    Vec3 position{};
    Quat rotation{};
    Vec3 linear_velocity{};
    Vec3 angular_velocity{};
    bool sleeping = false;
    float sleep_timer = 0.0f;
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
    return a.index == b.index && a.built_shape == b.built_shape && a.position == b.position &&
           a.rotation == b.rotation && a.linear_velocity == b.linear_velocity &&
           a.angular_velocity == b.angular_velocity && a.sleeping == b.sleeping &&
           a.sleep_timer == b.sleep_timer;
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
