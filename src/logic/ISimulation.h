#pragma once

#include "ecs/scene.hpp"

namespace openphysx {

// Logic contract. No window, GPU, or UI types live here.
class ISimulation
{
public:
    virtual ~ISimulation() = default;

    virtual void reset() = 0;
    virtual void step(float dt) = 0;
    virtual void seek(float time) = 0;

    virtual SimulationState& state() = 0;
    virtual const SimulationState& state() const = 0;

    virtual Scene& scene() = 0;
    virtual const Scene& scene() const = 0;

    virtual UUID active_id() const = 0;
    virtual void set_active(UUID id) = 0;
    virtual Entity active_entity() const = 0;

    // Draw pose of the active entity. Demo motion offsets this copy only.
    virtual RigidBody visual_body() const = 0;
};

} // namespace openphysx
