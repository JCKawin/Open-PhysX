#include "logic/Simulation.h"

#include <cmath>

namespace openphysx {

void Simulation::reset()
{
    state_ = SimulationState{};
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
    RigidBody body = state_.cube;
    if (state_.demo_motion)
        body.position.y += std::sin(state_.time * 3.0f) * 0.25f;
    return body;
}

} // namespace openphysx
