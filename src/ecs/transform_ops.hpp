#pragma once

#include "core/Math.h"
#include "core/Types.h"
#include "ecs/components/core.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace openphysx {

// Column-vector TRS. world = parent ∘ local: parent scale hits local translation, then
// parent rotation, then parent translation. Scale is per-axis in each object's frame.
// Shear is not stored. Invert fails when any parent scale axis is near zero.

inline constexpr float kInvertibleScale = 1.0e-8f;

inline bool ScaleInvertible(Vec3 scale)
{
    return std::fabs(scale.x) > kInvertibleScale && std::fabs(scale.y) > kInvertibleScale &&
           std::fabs(scale.z) > kInvertibleScale;
}

inline TransformComponent ComposeLocalIntoParent(const TransformComponent& parent, const TransformComponent& local)
{
    TransformComponent world;
    world.scale = {
        parent.scale.x * local.scale.x,
        parent.scale.y * local.scale.y,
        parent.scale.z * local.scale.z,
    };
    world.rotation = quat_normalize(quat_mul(parent.rotation, local.rotation));
    const Vec3 scaled{
        local.position.x * parent.scale.x,
        local.position.y * parent.scale.y,
        local.position.z * parent.scale.z,
    };
    world.position = vec_add(parent.position, quat_rotate(parent.rotation, scaled));
    return world;
}

struct UncomposeResult
{
    TransformComponent local{};
    bool invertible = true;
};

inline UncomposeResult UncomposeWorldFromParent(const TransformComponent& parent, const TransformComponent& world)
{
    UncomposeResult result;
    result.invertible = ScaleInvertible(parent.scale);
    auto axis_div = [](float value, float scale) {
        if (std::fabs(scale) <= kInvertibleScale)
            return 0.0f;
        return value / scale;
    };
    result.local.scale = {
        axis_div(world.scale.x, parent.scale.x),
        axis_div(world.scale.y, parent.scale.y),
        axis_div(world.scale.z, parent.scale.z),
    };
    result.local.rotation = quat_normalize(quat_mul(quat_conjugate(parent.rotation), world.rotation));
    const Vec3 relative = vec_sub(world.position, parent.position);
    const Vec3 unrotated = quat_rotate(quat_conjugate(parent.rotation), relative);
    result.local.position = {
        axis_div(unrotated.x, parent.scale.x),
        axis_div(unrotated.y, parent.scale.y),
        axis_div(unrotated.z, parent.scale.z),
    };
    return result;
}

// World-space edits for the transform tools. Each returns the new world transform of one
// root. The caller writes it back with Scene::SetWorldTransform.

inline TransformComponent MoveWorld(TransformComponent world, Vec3 delta)
{
    world.position = vec_add(world.position, delta);
    return world;
}

inline TransformComponent MoveLocal(TransformComponent world, Vec3 local_delta)
{
    world.position = vec_add(world.position, quat_rotate(world.rotation, local_delta));
    return world;
}

inline TransformComponent RotateWorld(TransformComponent world, Vec3 pivot, Quat rotation)
{
    world.position = vec_add(pivot, quat_rotate(rotation, vec_sub(world.position, pivot)));
    world.rotation = quat_normalize(quat_mul(rotation, world.rotation));
    return world;
}

inline TransformComponent RotateLocal(TransformComponent world, Quat local_rotation)
{
    world.rotation = quat_normalize(quat_mul(world.rotation, local_rotation));
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
