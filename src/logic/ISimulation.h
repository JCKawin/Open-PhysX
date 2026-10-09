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

    // The scene currently drawn and edited. While a simulation is running this is the
    // runtime copy. Stop throws that copy away and this returns the editor scene again.
    virtual Scene& scene() = 0;
    virtual const Scene& scene() const = 0;

    // The authored scene. The project file saves this, never the runtime copy.
    virtual Scene& editor_scene() = 0;
    virtual const Scene& editor_scene() const = 0;
    virtual bool simulating() const = 0;

    // Play copies the editor scene once. Pause keeps that copy. Stop discards it.
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    virtual UUID active_id() const = 0;
    virtual void set_active(UUID id) = 0;
    virtual Entity active_entity() const = 0;

    // Draw pose of the active entity. Demo motion offsets this copy only.
    virtual RigidBody visual_body() const = 0;
};

} // namespace openphysx
