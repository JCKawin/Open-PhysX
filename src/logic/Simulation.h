#pragma once

#include "logic/ISimulation.h"

namespace openphysx {

class Simulation final : public ISimulation
{
public:
    Simulation() = default;

    void reset() override;
    void step(float dt) override;
    void seek(float time) override;

    SimulationState& state() override { return state_; }
    const SimulationState& state() const override { return state_; }

    RigidBody visual_body() const override;

private:
    SimulationState state_{};
};

} // namespace openphysx
