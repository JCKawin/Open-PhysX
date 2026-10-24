#include "logic/Simulation.h"

#include "ecs/object_ops.hpp"
#include "ecs/physics_ops.hpp"
#include "ecs/pose.hpp"
#include "logic/rigid_step.hpp"

#include <algorithm>
#include <cmath>

namespace openphysx {

Simulation::Simulation()
{
    make_default_cube();
}

void Simulation::make_default_cube()
{
    active_ = AddBoxObject(editor_);
}

void Simulation::seed_runtime()
{
    if (!runtime_)
        return;
    SeedRuntimeBodies(*runtime_);
    scratch_.ensure(std::max<std::size_t>(runtime_->CreationOrder().size(), 8));
    physics_accum_ = 0.0f;
}

void Simulation::play()
{
    if (!runtime_)
    {
        runtime_ = editor_;
        seed_runtime();
    }
    state_.playing = true;
}

void Simulation::pause()
{
    state_.playing = false;
}

void Simulation::stop()
{
    state_.playing = false;
    runtime_.reset();
    physics_accum_ = 0.0f;
    last_sim_frames_ = 0;
}

void Simulation::reset()
{
    stop();
    state_ = SimulationState{};
    editor_ = Scene{};
    active_ = kNullUuid;
    make_default_cube();
}

void Simulation::set_display_step_cap(int cap)
{
    display_step_cap_ = std::max(1, cap);
}

void Simulation::advance_clock(float dt)
{
    if (state_.duration <= 0.0f)
    {
        state_.time = 0.0f;
        return;
    }
    if (state_.loop)
    {
        const float wrapped = std::fmod(dt, state_.duration);
        state_.time = wrapped < 0.0f ? wrapped + state_.duration : wrapped;
    }
    else
    {
        if (dt < 0.0f)
            state_.time = 0.0f;
        else if (dt > state_.duration)
            state_.time = state_.duration;
        else
            state_.time = dt;
    }
}

void Simulation::seek(float time)
{
    advance_clock(time);
    if (runtime_ && !in_step_)
        seed_runtime();
}

void Simulation::step(float dt)
{
    last_sim_frames_ = 0;
    if (!state_.playing)
        return;

    in_step_ = true;
    const RigidBodyWorld& world = scene().world();
    const float scaled = dt * state_.playback_speed * std::max(world.time_scale, 0.0f);
    physics_accum_ += scaled;

    const int cap = display_step_cap_;
    while (physics_accum_ >= kPhysicsDt && last_sim_frames_ < cap)
    {
        if (!world.muted && runtime_)
            SimulateFrame(*runtime_, kPhysicsDt, scratch_);
        physics_accum_ -= kPhysicsDt;
        ++last_sim_frames_;
        advance_clock(state_.time + kPhysicsDt);
        if (!state_.loop && state_.time >= state_.duration)
        {
            pause();
            break;
        }
    }
    if (physics_accum_ > kPhysicsDt * static_cast<float>(cap))
        physics_accum_ = 0.0f;
    in_step_ = false;
}

RigidBody Simulation::visual_body() const
{
    return visual_body_of(active_);
}

RigidBody Simulation::visual_body_of(UUID id) const
{
    const Scene& current = scene();
    const Entity entity = current.FindByUUID(id);
    if (!entity)
        return {};

    RigidBody body = WorldBody(current, entity);
    if (entity.Has<PhysicsBodyHandle>() && entity.Has<RigidBodyComponent>())
    {
        const PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
        body.position = handle.position;
        body.rotation = handle.rotation;
    }
    else if (id == active_ && state_.demo_motion && !entity.Has<RigidBodyComponent>())
    {
        body.position.y += std::sin(state_.time * 3.0f) * 0.25f;
    }
    return body;
}

} // namespace openphysx

