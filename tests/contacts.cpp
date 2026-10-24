#include "ecs/physics_ops.hpp"
#include "logic/Simulation.h"
#include "logic/contacts.hpp"
#include "logic/rigid_step.hpp"

#include <algorithm>
#include <doctest/doctest.h>

using namespace openphysx;

namespace {

Entity add_box(Scene& scene, const char* name, Vec3 position, Vec3 half, BodyType type)
{
    Entity entity = scene.CreateEntity(name);
    entity.Get<TransformComponent>().position = position;
    RigidBodyComponent body;
    body.type = type;
    body.linearDamping = 0.1f;
    body.angularDamping = 0.2f;
    entity.Add<RigidBodyComponent>(body);
    ColliderComponent collider;
    collider.shape = ShapeType::Box;
    collider.halfExtents = half;
    entity.Add<ColliderComponent>(collider);
    entity.Add<PhysicsMaterialComponent>(PhysicsMaterialComponent{0.6f, 0.0f, 0.0f});
    return entity;
}

Entity add_sphere(Scene& scene, const char* name, Vec3 position, float radius, BodyType type)
{
    Entity entity = scene.CreateEntity(name);
    entity.Get<TransformComponent>().position = position;
    RigidBodyComponent body;
    body.type = type;
    body.linearDamping = 0.1f;
    body.angularDamping = 0.2f;
    entity.Add<RigidBodyComponent>(body);
    ColliderComponent collider;
    collider.shape = ShapeType::Sphere;
    collider.radius = radius;
    entity.Add<ColliderComponent>(collider);
    entity.Add<PhysicsMaterialComponent>(PhysicsMaterialComponent{0.4f, 0.0f, 0.0f});
    return entity;
}

void run_seconds(Simulation& simulation, float seconds)
{
    simulation.set_display_step_cap(64);
    simulation.step(seconds);
}

} // namespace

TEST_CASE("sphere-sphere contact normal points from A to B")
{
    Scene scene;
    Entity a = add_sphere(scene, "A", {0.0f, 0.0f, 0.0f}, 0.5f, BodyType::Dynamic);
    Entity b = add_sphere(scene, "B", {0.6f, 0.0f, 0.0f}, 0.5f, BodyType::Dynamic);
    SeedRuntimeBodies(scene);
    PhysicsScratch scratch;
    scratch.ensure(8);
    BuildContacts(scene, scratch);
    const std::vector<ContactPoint> points = CopyContacts(scratch);
    REQUIRE_FALSE(points.empty());
    const ContactPoint& c = points.front();
    const UUID lo = std::min(a.GetUUID(), b.GetUUID());
    const UUID hi = std::max(a.GetUUID(), b.GetUUID());
    CHECK(c.a == lo);
    CHECK(c.b == hi);
    CHECK(c.penetration > 0.0f);
    const Vec3 delta = vec_sub(scene.FindByUUID(hi).Get<PhysicsBodyHandle>().position,
                               scene.FindByUUID(lo).Get<PhysicsBodyHandle>().position);
    CHECK(vec_dot(c.normal, delta) > 0.0f);
}

TEST_CASE("sphere on a floor has a normal pointing toward the sphere")
{
    Scene scene;
    Entity floor = add_box(scene, "Floor", {0.0f, -0.5f, 0.0f}, {20.0f, 0.5f, 20.0f}, BodyType::Static);
    Entity ball = add_sphere(scene, "Ball", {0.0f, 0.4f, 0.0f}, 0.5f, BodyType::Dynamic);
    SeedRuntimeBodies(scene);
    PhysicsScratch scratch;
    scratch.ensure(8);
    BuildContacts(scene, scratch);
    const std::vector<ContactPoint> points = CopyContacts(scratch);
    REQUIRE_FALSE(points.empty());
    Vec3 toward_sphere{};
    for (const ContactPoint& c : points)
    {
        if (c.a == ball.GetUUID() && c.b == floor.GetUUID())
            toward_sphere = vec_neg(c.normal);
        else if (c.a == floor.GetUUID() && c.b == ball.GetUUID())
            toward_sphere = c.normal;
    }
    CHECK(toward_sphere.y == doctest::Approx(1.0f).epsilon(0.15));
}

TEST_CASE("an active box dropped on a passive floor rests within 0.01")
{
    Simulation simulation;
    simulation.reset();
    Scene& scene = simulation.editor_scene();
    add_box(scene, "Floor", {0.0f, -0.5f, 0.0f}, {20.0f, 0.5f, 20.0f}, BodyType::Static);
    Entity box = add_box(scene, "Drop", {0.0f, 3.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, BodyType::Dynamic);
    const UUID id = box.GetUUID();
    simulation.play();
    run_seconds(simulation, 1.0f);
    const float y = simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y;
    CHECK(y == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("stacked spheres stay stacked for one second")
{
    Simulation simulation;
    simulation.reset();
    Scene& scene = simulation.editor_scene();
    add_box(scene, "Floor", {0.0f, -0.5f, 0.0f}, {20.0f, 0.5f, 20.0f}, BodyType::Static);
    Entity lower = add_sphere(scene, "Lower", {0.0f, 0.5f, 0.0f}, 0.5f, BodyType::Dynamic);
    Entity upper = add_sphere(scene, "Upper", {0.0f, 1.5f, 0.0f}, 0.5f, BodyType::Dynamic);
    const UUID lo = lower.GetUUID();
    const UUID hi = upper.GetUUID();
    simulation.play();
    run_seconds(simulation, 1.0f);
    const float y0 = simulation.scene().FindByUUID(lo).Get<PhysicsBodyHandle>().position.y;
    const float y1 = simulation.scene().FindByUUID(hi).Get<PhysicsBodyHandle>().position.y;
    CHECK(y0 == doctest::Approx(0.5f).epsilon(0.08));
    CHECK(y1 == doctest::Approx(1.5f).epsilon(0.12));
    CHECK(y1 > y0 + 0.6f);
}

TEST_CASE("a sleeping body wakes when another body hits it")
{
    Simulation simulation;
    simulation.reset();
    Scene& scene = simulation.editor_scene();
    add_box(scene, "Floor", {0.0f, -0.5f, 0.0f}, {20.0f, 0.5f, 20.0f}, BodyType::Static);
    Entity sleeper = add_sphere(scene, "Sleep", {0.0f, 0.5f, 0.0f}, 0.5f, BodyType::Dynamic);
    sleeper.Get<RigidBodyComponent>().startAsleep = true;
    Entity striker = add_sphere(scene, "Hit", {0.0f, 3.0f, 0.0f}, 0.5f, BodyType::Dynamic);
    const UUID sleep_id = sleeper.GetUUID();
    (void)striker;
    simulation.play();
    CHECK(simulation.scene().FindByUUID(sleep_id).Get<PhysicsBodyHandle>().sleeping);
    run_seconds(simulation, 1.0f);
    CHECK_FALSE(simulation.scene().FindByUUID(sleep_id).Get<PhysicsBodyHandle>().sleeping);
}

TEST_CASE("contact replay is deterministic")
{
    auto rest_y = []() {
        Simulation simulation;
        simulation.reset();
        Scene& scene = simulation.editor_scene();
        add_box(scene, "Floor", {0.0f, -0.5f, 0.0f}, {20.0f, 0.5f, 20.0f}, BodyType::Static);
        Entity box = add_box(scene, "Drop", {0.0f, 2.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, BodyType::Dynamic);
        const UUID id = box.GetUUID();
        simulation.play();
        run_seconds(simulation, 1.0f);
        return simulation.scene().FindByUUID(id).Get<PhysicsBodyHandle>().position.y;
    };
    CHECK(rest_y() == doctest::Approx(rest_y()));
}

TEST_CASE("scratch capacity is enough that a substep does not allocate")
{
    Scene scene;
    add_box(scene, "Floor", {0.0f, -0.5f, 0.0f}, {20.0f, 0.5f, 20.0f}, BodyType::Static);
    add_box(scene, "Drop", {0.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, BodyType::Dynamic);
    SeedRuntimeBodies(scene);
    PhysicsScratch scratch;
    scratch.ensure(scene.CreationOrder().size());
    SubstepScene(scene, kPhysicsDt, scratch);
    const std::size_t pairs = scratch.pairs.capacity();
    const std::size_t contacts = scratch.contacts.capacity();
    SubstepScene(scene, kPhysicsDt, scratch);
    CHECK(scratch.pairs.capacity() == pairs);
    CHECK(scratch.contacts.capacity() == contacts);
}
