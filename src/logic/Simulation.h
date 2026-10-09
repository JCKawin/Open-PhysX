#pragma once

#include "logic/ISimulation.h"

namespace openphysx {

class Simulation final : public ISimulation
{
public:
    Simulation();

    void reset() override;
    void step(float dt) override;
    void seek(float time) override;

    SimulationState& state() override { return state_; }
    const SimulationState& state() const override { return state_; }

    Scene& scene() override { return scene_; }
    const Scene& scene() const override { return scene_; }

    UUID active_id() const override { return active_; }
    void set_active(UUID id) override { active_ = id; }
    Entity active_entity() const override { return scene_.FindByUUID(active_); }

    RigidBody visual_body() const override;

private:
    void make_default_cube();

    SimulationState state_{};
    Scene scene_{};
    UUID active_ = kNullUuid;
};

} // namespace openphysx
