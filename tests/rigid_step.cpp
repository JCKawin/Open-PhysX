#include "ecs/physics_ops.hpp"
#include "logic/Simulation.h"
#include "logic/rigid_step.hpp"

#include <doctest/doctest.h>

using namespace openphysx;

namespace {

Entity add_active(Scene& scene, Vec3 position, bool gravity)
{
    Entity entity = scene.CreateEntity("Body");
    entity.Get<TransformComponent>().position = position;
    RigidBodyComponent body;
    body.type = BodyType::Dynamic;
    body.mass = 1.0f;
    body.gravityEnabled = gravity;
    body.linearDamping = 0.0f;
    body.angularDamping = 0.0f;
    entity.Add<RigidBodyComponent>(body);
    ColliderComponent collider;
    collider.shape = ShapeType::Box;
    entity.Add<ColliderComponent>(collider);
    return entity;
}

} // namespace

TEST_CASE("gravity off leaves an idle active body still")
{
    Simulation simulation;
    simulation.reset();
    Scene& scene = simulation.editor_scene();
    Entity entity = add_active(scene, {0.0f, 2.0f, 0.0f}, false);
    const UUID id = entity.GetUUID();
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    CHECK(simulation.last_sim_frames() == 8);
    const PhysicsBodyHandle& handle = simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>();
    CHECK(handle.position.y == doctest::Approx(2.0f).epsilon(1.0e-4));
    CHECK(handle.linear_velocity.y == doctest::Approx(0.0f).epsilon(1.0e-4));
}

TEST_CASE("gravity on accelerates an active body along -Y")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 10.0f, 0.0f}, true);
    const UUID id = entity.GetUUID();
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    const PhysicsBodyHandle& handle = simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>();
    const int substeps = simulation.scene().world().substeps;
    const int n = 8 * substeps;
    const float h = kPhysicsDt / static_cast<float>(substeps);
    CHECK(handle.linear_velocity.y == doctest::Approx(-9.81f * static_cast<float>(n) * h).epsilon(1.0e-3));
    CHECK(handle.position.y ==
          doctest::Approx(10.0f + -9.81f * h * h * static_cast<float>(n * (n + 1)) * 0.5f).epsilon(1.0e-3));
    CHECK(simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position.y == doctest::Approx(10.0f));
}

TEST_CASE("a passive body stays on its authored pose")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {1.0f, 4.0f, 0.0f}, true);
    entity.Get<RigidBodyComponent>().type = BodyType::Static;
    const UUID id = entity.GetUUID();
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    const PhysicsBodyHandle& handle = simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>();
    CHECK(handle.position.y == doctest::Approx(4.0f));
    CHECK(handle.linear_velocity.y == doctest::Approx(0.0f));
}

TEST_CASE("a kinematic body follows authored motion and reports velocity")
{
    Scene scene;
    Entity entity = add_active(scene, {0.0f, 1.0f, 0.0f}, false);
    entity.Get<RigidBodyComponent>().type = BodyType::Kinematic;
    SeedRuntimeBodies(scene);
    entity.Get<TransformComponent>().position.x = 2.0f;
    SubstepScene(scene, kPhysicsDt);
    const PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
    CHECK(handle.position.x == doctest::Approx(2.0f));
    CHECK(handle.linear_velocity.x == doctest::Approx(2.0f / kPhysicsDt).epsilon(1.0e-3));
}

TEST_CASE("a 1s hitch runs at most the display-step cap")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 0.0f, 0.0f}, true);
    const UUID id = entity.GetUUID();
    simulation.set_display_step_cap(4);
    simulation.play();
    simulation.step(1.0f);
    CHECK(simulation.last_sim_frames() == 4);
    const float t = kPhysicsDt * 4.0f;
    const PhysicsBodyHandle& handle = simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>();
    CHECK(handle.linear_velocity.y == doctest::Approx(-9.81f * t).epsilon(1.0e-3));
    CHECK(handle.position.y > -9.81f * 0.5f);
}

TEST_CASE("substeps run inside each physics frame")
{
    Scene scene;
    Entity entity = add_active(scene, {0.0f, 0.0f, 0.0f}, true);
    scene.world().substeps = 10;
    SeedRuntimeBodies(scene);
    SimulateFrame(scene, kPhysicsDt);
    const float t = kPhysicsDt;
    const PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
    CHECK(handle.linear_velocity.y == doctest::Approx(-9.81f * t).epsilon(1.0e-3));
}

TEST_CASE("mute and pause do not integrate")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 3.0f, 0.0f}, true);
    const UUID id = entity.GetUUID();
    simulation.editor_scene().world().muted = true;
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    CHECK(simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y == doctest::Approx(3.0f));
    simulation.stop();
    simulation.editor_scene().world().muted = false;
    simulation.play();
    simulation.step(kPhysicsDt * 8.0f);
    const float falling = simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y;
    CHECK(falling < 3.0f);
    simulation.pause();
    simulation.step(kPhysicsDt * 8.0f);
    CHECK(simulation.last_sim_frames() == 0);
    CHECK(simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y == doctest::Approx(falling));
}

TEST_CASE("seek while playing reseeds instead of integrating the jump")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 5.0f, 0.0f}, true);
    const UUID id = entity.GetUUID();
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    CHECK(simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y < 5.0f);
    simulation.seek(0.0f);
    CHECK(simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y == doctest::Approx(5.0f));
}

TEST_CASE("the same steps replay to the same pose")
{
    auto run = []() {
        Simulation simulation;
        simulation.reset();
        Entity entity = add_active(simulation.editor_scene(), {0.0f, 8.0f, 0.0f}, true);
        const UUID id = entity.GetUUID();
        simulation.set_display_step_cap(8);
        simulation.play();
        simulation.step(kPhysicsDt * 8.0f);
        return simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y;
    };
    CHECK(run() == doctest::Approx(run()));
}

TEST_CASE("locking Y blocks gravity")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 2.0f, 0.0f}, true);
    entity.Get<RigidBodyComponent>().linearFactor = {1.0f, 0.0f, 1.0f};
    const UUID id = entity.GetUUID();
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    CHECK(simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y == doctest::Approx(2.0f));
}

TEST_CASE("demo sine does not offset a rigid body")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 1.0f, 0.0f}, false);
    const UUID id = entity.GetUUID();
    simulation.state().demo_motion = true;
    simulation.play();
    simulation.state().time = 1.0f;
    const RigidBody drawn = simulation.visual_body_of(id);
    CHECK(drawn.position.y == doctest::Approx(1.0f));
}

TEST_CASE("a sleeping body does not fall until it is not start-asleep")
{
    Simulation simulation;
    simulation.reset();
    Entity entity = add_active(simulation.editor_scene(), {0.0f, 6.0f, 0.0f}, true);
    entity.Get<RigidBodyComponent>().startAsleep = true;
    const UUID id = entity.GetUUID();
    simulation.play();
    simulation.set_display_step_cap(8);
    simulation.step(kPhysicsDt * 8.0f);
    CHECK(simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y == doctest::Approx(6.0f));
}
