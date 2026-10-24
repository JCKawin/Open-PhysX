#pragma once

#include "core/Math.h"
#include "ecs/components/physics.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/scene.hpp"

namespace openphysx {

inline bool IsActiveBody(BodyType type)
{
    return type == BodyType::Dynamic;
}

// The first solver only contacts boxes and spheres. Hull and triangle mesh stay as metadata.
inline bool ShapeSupportedForActive(ShapeType shape)
{
    return shape == ShapeType::Box || shape == ShapeType::Sphere;
}

enum class PhysicsAuthoringError
{
    None,
    NonPositiveMass,
    UnsupportedActiveShape,
};

inline PhysicsAuthoringError ValidateRigidBody(const RigidBodyComponent& body, const ColliderComponent* collider)
{
    if (IsActiveBody(body.type) && body.mass <= 0.0f)
        return PhysicsAuthoringError::NonPositiveMass;
    if (collider != nullptr && IsActiveBody(body.type) && !ShapeSupportedForActive(collider->shape))
        return PhysicsAuthoringError::UnsupportedActiveShape;
    return PhysicsAuthoringError::None;
}

inline void TouchWorld(Scene& scene)
{
    scene.bump_physics();
}

inline void TouchCollider(Scene& scene, Entity entity)
{
    if (!entity || !entity.Has<ColliderComponent>())
        return;
    entity.Get<ColliderComponent>().shape_dirty = true;
    scene.bump_physics();
}

inline void TouchRigidBody(Scene& scene, Entity entity)
{
    if (!entity || !entity.Has<RigidBodyComponent>())
        return;
    scene.bump_physics();
    if (entity.Has<ColliderComponent>())
        entity.Get<ColliderComponent>().shape_dirty = true;
}

inline bool SanitizeRigidBody(RigidBodyComponent& body, ColliderComponent* collider)
{
    bool changed = false;
    if (IsActiveBody(body.type) && (!(body.mass > 0.0f)))
    {
        body.mass = 1.0f;
        changed = true;
    }
    if (collider != nullptr && IsActiveBody(body.type) && !ShapeSupportedForActive(collider->shape))
    {
        collider->shape = ShapeType::Box;
        collider->shape_dirty = true;
        changed = true;
    }
    return changed;
}

inline void SanitizeWorld(RigidBodyWorld& world)
{
    if (!(world.time_scale > 0.0f))
        world.time_scale = 1.0f;
    if (world.substeps < 1)
        world.substeps = 1;
    if (world.solver_iterations < 1)
        world.solver_iterations = 1;
}

inline int RebuildDirtyShapes(Scene& scene)
{
    int rebuilt = 0;
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity || !entity.Has<ColliderComponent>())
            continue;
        ColliderComponent& collider = entity.Get<ColliderComponent>();
        if (!collider.shape_dirty)
            continue;
        if (entity.Has<RigidBodyComponent>())
            SanitizeRigidBody(entity.Get<RigidBodyComponent>(), &collider);
        if (!entity.Has<PhysicsBodyHandle>())
            entity.Add<PhysicsBodyHandle>();
        PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
        handle.built_shape = static_cast<int>(collider.shape);
        handle.index = rebuilt;
        collider.shape_dirty = false;
        ++rebuilt;
    }
    return rebuilt;
}

inline void EnsureRigidBodyAuthoring(Scene& scene, Entity entity)
{
    if (!entity)
        return;
    if (!entity.Has<RigidBodyComponent>())
        entity.Add<RigidBodyComponent>();
    if (!entity.Has<ColliderComponent>())
    {
        ColliderComponent collider;
        if (entity.Has<PrimitiveBoxComponent>())
            collider.halfExtents = vec_scale(entity.Get<PrimitiveBoxComponent>().size, 0.5f);
        entity.Add<ColliderComponent>(collider);
    }
    if (!entity.Has<PhysicsMaterialComponent>())
        entity.Add<PhysicsMaterialComponent>();
    TouchRigidBody(scene, entity);
}

} // namespace openphysx
