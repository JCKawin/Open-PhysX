#include "logic/Simulation.h"

#include "ecs/pose.hpp"

#include <cmath>

namespace openphysx {

Simulation::Simulation()
{
    make_default_cube();
}

void Simulation::make_default_cube()
{
    Entity cube = scene_.CreateEntity("Cube");
    cube.Get<TransformComponent>().position = {0.0f, 1.0f, 0.0f};
    cube.Add<PrimitiveBoxComponent>();
    cube.Add<SelectionOutlineTag>();
    active_ = cube.GetUUID();
}

void Simulation::reset()
{
    state_ = SimulationState{};
    scene_ = Scene{};
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
        state_.playing = false;
}

RigidBody Simulation::visual_body() const
{
    const Entity entity = active_entity();
    if (!entity)
        return {};

    RigidBody body = EntityPose(entity);
    if (state_.demo_motion)
        body.position.y += std::sin(state_.time * 3.0f) * 0.25f;
    return body;
}

} // namespace openphysx
