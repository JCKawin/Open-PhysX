#pragma once

#include "core/Math.h"
#include "ecs/physics_ops.hpp"
#include "ecs/pose.hpp"
#include "logic/contacts.hpp"

#include <algorithm>
#include <cmath>

namespace openphysx {

inline constexpr float kPhysicsDt = 1.0f / 60.0f;
inline constexpr int kDefaultDisplayStepCap = 4;

inline void SeedRuntimeBody(Scene& scene, Entity entity)
{
    if (!entity || !entity.Has<RigidBodyComponent>())
        return;
    if (!entity.Has<PhysicsBodyHandle>())
        entity.Add<PhysicsBodyHandle>();
    const RigidBodyComponent& body = entity.Get<RigidBodyComponent>();
    const TransformComponent world = scene.GetWorldTransform(entity);
    PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
    handle.position = world.position;
    handle.rotation = world.rotation;
    handle.linear_velocity = vec_mul(body.linearVelocity, body.linearFactor);
    handle.angular_velocity = vec_mul(body.angularVelocity, body.angularFactor);
    handle.sleeping = body.startAsleep;
}

inline void SeedRuntimeBodies(Scene& scene)
{
    RebuildDirtyShapes(scene);
    for (const UUID id : scene.CreationOrder())
        SeedRuntimeBody(scene, scene.FindByUUID(id));
}

inline void ApplyForces(PhysicsBodyHandle& handle, const RigidBodyComponent& body, const RigidBodyWorld& world, float dt)
{
    if (handle.sleeping)
        return;
    Vec3 gravity = body.gravityEnabled ? world.gravity : Vec3{};
    gravity = vec_mul(gravity, body.linearFactor);
    handle.linear_velocity = vec_add(handle.linear_velocity, vec_scale(gravity, dt));
    const float lin_damp = std::exp(-std::max(body.linearDamping, 0.0f) * dt);
    handle.linear_velocity = vec_scale(handle.linear_velocity, lin_damp);
    handle.linear_velocity = vec_mul(handle.linear_velocity, body.linearFactor);

    const float ang_damp = std::exp(-std::max(body.angularDamping, 0.0f) * dt);
    handle.angular_velocity = vec_scale(handle.angular_velocity, ang_damp);
    handle.angular_velocity = vec_mul(handle.angular_velocity, body.angularFactor);
}

inline void IntegratePosition(PhysicsBodyHandle& handle, float dt)
{
    if (handle.sleeping)
        return;
    handle.position = vec_add(handle.position, vec_scale(handle.linear_velocity, dt));
    const Vec3 w = handle.angular_velocity;
    const Quat omega_q{w.x, w.y, w.z, 0.0f};
    const Quat q = handle.rotation;
    const Quat dq = quat_mul(omega_q, q);
    handle.rotation = quat_normalize({
        q.x + 0.5f * dt * dq.x,
        q.y + 0.5f * dt * dq.y,
        q.z + 0.5f * dt * dq.z,
        q.w + 0.5f * dt * dq.w,
    });
}

inline void IntegrateActive(PhysicsBodyHandle& handle, const RigidBodyComponent& body, const RigidBodyWorld& world, float dt)
{
    ApplyForces(handle, body, world, dt);
    IntegratePosition(handle, dt);
}

inline void SnapPassive(PhysicsBodyHandle& handle, const TransformComponent& world)
{
    handle.position = world.position;
    handle.rotation = world.rotation;
    handle.linear_velocity = {};
    handle.angular_velocity = {};
}

inline void FollowKinematic(PhysicsBodyHandle& handle, const TransformComponent& world, float dt)
{
    if (dt > 0.0f)
    {
        handle.linear_velocity = vec_scale(vec_sub(world.position, handle.position), 1.0f / dt);
        // Small-angle estimate: 2 * imag(q_prev^-1 * q_world) / dt
        const Quat delta = quat_mul(quat_conjugate(handle.rotation), world.rotation);
        handle.angular_velocity = vec_scale({delta.x, delta.y, delta.z}, 2.0f / dt);
    }
    else
    {
        handle.linear_velocity = {};
        handle.angular_velocity = {};
    }
    handle.position = world.position;
    handle.rotation = world.rotation;
}

inline void SubstepScene(Scene& scene, float dt, PhysicsScratch& scratch)
{
    if (!(dt > 0.0f))
        return;
    const RigidBodyWorld& world = scene.world();
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity || !entity.Has<RigidBodyComponent>())
            continue;
        if (!entity.Has<PhysicsBodyHandle>())
            SeedRuntimeBody(scene, entity);
        PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
        const RigidBodyComponent& body = entity.Get<RigidBodyComponent>();
        const TransformComponent authored = scene.GetWorldTransform(entity);
        if (body.type == BodyType::Static)
            SnapPassive(handle, authored);
        else if (body.type == BodyType::Kinematic)
            FollowKinematic(handle, authored, dt);
        else
            ApplyForces(handle, body, world, dt);
    }
    SolveSceneContacts(scene, dt, scratch);
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity || !entity.Has<RigidBodyComponent>() || !entity.Has<PhysicsBodyHandle>())
            continue;
        if (entity.Get<RigidBodyComponent>().type == BodyType::Dynamic)
            IntegratePosition(entity.Get<PhysicsBodyHandle>(), dt);
    }
    UpdateSleep(scratch, dt);
}

inline void SubstepScene(Scene& scene, float dt)
{
    static PhysicsScratch scratch;
    scratch.ensure(std::max<std::size_t>(scene.CreationOrder().size(), 8));
    SubstepScene(scene, dt, scratch);
}

inline void SimulateFrame(Scene& scene, float frame_dt, PhysicsScratch& scratch)
{
    SanitizeWorld(scene.world());
    const int substeps = scene.world().substeps;
    const float dt = frame_dt / static_cast<float>(substeps);
    scratch.ensure(std::max<std::size_t>(scene.CreationOrder().size(), 8));
    for (int i = 0; i < substeps; ++i)
        SubstepScene(scene, dt, scratch);
}

inline void SimulateFrame(Scene& scene, float frame_dt)
{
    static PhysicsScratch scratch;
    SimulateFrame(scene, frame_dt, scratch);
}

} // namespace openphysx
