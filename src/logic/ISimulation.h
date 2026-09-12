#pragma once

#include "core/Types.h"

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

    // Pose the renderer should draw (includes timeline evaluation).
    virtual RigidBody visual_body() const = 0;
};

} // namespace openphysx
