#pragma once

#include "core/Math.h"
#include "core/Types.h"
#include "ecs/components/core.hpp"

#include <algorithm>
#include <vector>

namespace openphysx {

// World-space edits for the transform tools. Each returns the new world transform of one
// root. The caller writes it back with Scene::SetWorldTransform.

inline TransformComponent MoveWorld(TransformComponent world, Vec3 delta)
{
    world.position = vec_add(world.position, delta);
    return world;
}

inline TransformComponent RotateWorld(TransformComponent world, Vec3 pivot, Quat rotation)
{
    world.position = vec_add(pivot, quat_rotate(rotation, vec_sub(world.position, pivot)));
    world.rotation = quat_normalize(quat_mul(rotation, world.rotation));
    return world;
}

// Multiplies each axis about the pivot. A scale component never drops below `min_scale`.
inline TransformComponent ScaleWorld(TransformComponent world, Vec3 pivot, Vec3 factor, float min_scale)
{
    world.position = {
        pivot.x + (world.position.x - pivot.x) * factor.x,
        pivot.y + (world.position.y - pivot.y) * factor.y,
        pivot.z + (world.position.z - pivot.z) * factor.z,
    };
    world.scale = {
        std::max(min_scale, world.scale.x * factor.x),
        std::max(min_scale, world.scale.y * factor.y),
        std::max(min_scale, world.scale.z * factor.z),
    };
    return world;
}

// The pivot for rotating or scaling several objects together, as Blender uses the median.
inline Vec3 MeanPosition(const std::vector<TransformComponent>& worlds)
{
    if (worlds.empty())
        return {};
    Vec3 sum{};
    for (const TransformComponent& world : worlds)
        sum = vec_add(sum, world.position);
    return vec_scale(sum, 1.0f / static_cast<float>(worlds.size()));
}

} // namespace openphysx
