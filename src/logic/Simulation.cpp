#include "logic/Simulation.h"

#include "ecs/object_ops.hpp"
#include "ecs/pose.hpp"

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

void Simulation::play()
{
    if (!runtime_)
        runtime_ = editor_;
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
}

void Simulation::reset()
{
    stop();
    state_ = SimulationState{};
    editor_ = Scene{};
    active_ = kNullUuid;
    make_default_cube();
}

void Simulation::seek(float time)
{
    if (state_.duration <= 0.0f)
    {
        state_.time = 0.0f;
        return;
    }

    if (state_.loop)
    {
        const float wrapped = std::fmod(time, state_.duration);
        state_.time = wrapped < 0.0f ? wrapped + state_.duration : wrapped;
    }
    else
    {
        if (time < 0.0f)
            state_.time = 0.0f;
        else if (time > state_.duration)
            state_.time = state_.duration;
        else
            state_.time = time;
    }
}

void Simulation::step(float dt)
{
    if (!state_.playing)
        return;

    seek(state_.time + dt * state_.playback_speed);

    if (!state_.loop && state_.time >= state_.duration)
        pause();
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
    if (id == active_ && state_.demo_motion)
        body.position.y += std::sin(state_.time * 3.0f) * 0.25f;
    return body;
}

} // namespace openphysx
