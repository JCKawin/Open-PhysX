#pragma once

#include "logic/ISimulation.h"

#include <optional>

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

    Scene& scene() override { return runtime_ ? *runtime_ : editor_; }
    const Scene& scene() const override { return runtime_ ? *runtime_ : editor_; }
    Scene& editor_scene() override { return editor_; }
    const Scene& editor_scene() const override { return editor_; }
    bool simulating() const override { return runtime_.has_value(); }

    void play() override;
    void pause() override;
    void stop() override;

    UUID active_id() const override { return active_; }
    void set_active(UUID id) override { active_ = id; }
    Entity active_entity() const override { return scene().FindByUUID(active_); }

    RigidBody visual_body() const override;
    RigidBody visual_body_of(UUID id) const override;

private:
    void make_default_cube();

    SimulationState state_{};
    Scene editor_{};
    std::optional<Scene> runtime_;
    UUID active_ = kNullUuid;
};

} // namespace openphysx
