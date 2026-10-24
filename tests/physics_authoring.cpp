#include "ecs/physics_ops.hpp"
#include "ecs/scene.hpp"
#include "persistence/scene_serializer.hpp"

#include <doctest/doctest.h>

using namespace openphysx;

namespace {

Entity make_body(Scene& scene, BodyType type, ShapeType shape)
{
    Entity entity = scene.CreateEntity("Body");
    RigidBodyComponent body;
    body.type = type;
    entity.Add<RigidBodyComponent>(body);
    ColliderComponent collider;
    collider.shape = shape;
    entity.Add<ColliderComponent>(collider);
    return entity;
}

} // namespace

TEST_CASE("active mass at or below zero is clamped to 1")
{
    RigidBodyComponent body;
    body.type = BodyType::Dynamic;
    body.mass = 0.0f;
    CHECK(ValidateRigidBody(body, nullptr) == PhysicsAuthoringError::NonPositiveMass);
    CHECK(SanitizeRigidBody(body, nullptr));
    CHECK(body.mass == doctest::Approx(1.0f));
    CHECK(ValidateRigidBody(body, nullptr) == PhysicsAuthoringError::None);
}

TEST_CASE("an active triangle mesh is rejected and becomes a box")
{
    RigidBodyComponent body;
    body.type = BodyType::Dynamic;
    ColliderComponent collider;
    collider.shape = ShapeType::TriangleMesh;
    CHECK(ValidateRigidBody(body, &collider) == PhysicsAuthoringError::UnsupportedActiveShape);
    CHECK(SanitizeRigidBody(body, &collider));
    CHECK(collider.shape == ShapeType::Box);
    CHECK(collider.shape_dirty);
}

TEST_CASE("an active hull is rejected; a passive triangle mesh is kept")
{
    RigidBodyComponent active;
    active.type = BodyType::Dynamic;
    ColliderComponent hull;
    hull.shape = ShapeType::Hull;
    CHECK(ValidateRigidBody(active, &hull) == PhysicsAuthoringError::UnsupportedActiveShape);
    SanitizeRigidBody(active, &hull);
    CHECK(hull.shape == ShapeType::Box);

    RigidBodyComponent passive;
    passive.type = BodyType::Static;
    ColliderComponent mesh;
    mesh.shape = ShapeType::TriangleMesh;
    CHECK(ValidateRigidBody(passive, &mesh) == PhysicsAuthoringError::None);
    CHECK_FALSE(SanitizeRigidBody(passive, &mesh));
    CHECK(mesh.shape == ShapeType::TriangleMesh);
}

TEST_CASE("shape rebuild walks only dirty colliders")
{
    Scene scene;
    Entity dirty = make_body(scene, BodyType::Dynamic, ShapeType::Box);
    Entity clean = make_body(scene, BodyType::Static, ShapeType::Sphere);
    clean.Get<ColliderComponent>().shape_dirty = false;
    CHECK(RebuildDirtyShapes(scene) == 1);
    CHECK_FALSE(dirty.Get<ColliderComponent>().shape_dirty);
    CHECK(dirty.Get<PhysicsBodyHandle>().built_shape == static_cast<int>(ShapeType::Box));
    CHECK_FALSE(clean.Has<PhysicsBodyHandle>());
    CHECK(RebuildDirtyShapes(scene) == 0);
    clean.Get<ColliderComponent>().shape = ShapeType::Box;
    TouchCollider(scene, clean);
    CHECK(RebuildDirtyShapes(scene) == 1);
    CHECK(clean.Get<PhysicsBodyHandle>().built_shape == static_cast<int>(ShapeType::Box));
}

TEST_CASE("world and body settings round-trip through the scene file")
{
    Scene scene;
    scene.world().gravity = {0.0f, -4.9f, 0.0f};
    scene.world().time_scale = 0.5f;
    scene.world().substeps = 4;
    scene.world().solver_iterations = 8;
    scene.world().muted = true;
    Entity entity = make_body(scene, BodyType::Kinematic, ShapeType::Sphere);
    RigidBodyComponent& body = entity.Get<RigidBodyComponent>();
    body.mass = 2.5f;
    body.linearDamping = 0.2f;
    body.linearFactor = {1.0f, 0.0f, 1.0f};
    body.canSleep = false;
    body.startAsleep = true;
    entity.Get<ColliderComponent>().radius = 1.25f;

    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(WriteScene(scene), loaded, report));
    CHECK(loaded.world() == scene.world());
    Entity again = loaded.FindByUUID(entity.GetUUID());
    REQUIRE(again);
    CHECK(again.Get<RigidBodyComponent>().type == BodyType::Kinematic);
    CHECK(again.Get<RigidBodyComponent>().mass == doctest::Approx(2.5f));
    CHECK(again.Get<RigidBodyComponent>().linearFactor.y == doctest::Approx(0.0f));
    CHECK_FALSE(again.Get<RigidBodyComponent>().canSleep);
    CHECK(again.Get<RigidBodyComponent>().startAsleep);
    CHECK(again.Get<ColliderComponent>().shape == ShapeType::Sphere);
    CHECK(again.Get<ColliderComponent>().radius == doctest::Approx(1.25f));
}

TEST_CASE("a file without rigid_world keeps the default Y-up gravity")
{
    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(nlohmann::json{{"version", 2}, {"entities", nlohmann::json::array()}}, loaded, report));
    CHECK(loaded.world().gravity.y == doctest::Approx(-9.81f));
    CHECK(loaded.world().substeps == 10);
}

TEST_CASE("load sanitizes an active body with zero mass and a triangle mesh")
{
    const nlohmann::json json = {
        {"version", 2},
        {"entities",
         nlohmann::json::array(
             {nlohmann::json{
                 {"id", "0000000000000010"},
                 {"components",
                  {{"Tag", {{"name", "Bad"}}},
                   {"RigidBody", {{"type", "Dynamic"}, {"mass", 0.0f}}},
                   {"Collider", {{"shape", "TriangleMesh"}}}}}}})}};
    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(json, loaded, report));
    Entity entity = loaded.FindByName("Bad");
    REQUIRE(entity);
    CHECK(entity.Get<RigidBodyComponent>().mass == doctest::Approx(1.0f));
    CHECK(entity.Get<ColliderComponent>().shape == ShapeType::Box);
    CHECK_FALSE(report.entries.empty());
}

TEST_CASE("touching a collider bumps the physics revision")
{
    Scene scene;
    Entity entity = make_body(scene, BodyType::Dynamic, ShapeType::Box);
    const std::uint64_t before = scene.physics_revision();
    TouchCollider(scene, entity);
    CHECK(scene.physics_revision() == before + 1);
    TouchWorld(scene);
    CHECK(scene.physics_revision() == before + 2);
}

TEST_CASE("adding authoring components leaves a dirty box collider")
{
    Scene scene;
    Entity entity = scene.CreateEntity("Cube");
    entity.Add<PrimitiveBoxComponent>();
    EnsureRigidBodyAuthoring(scene, entity);
    CHECK(entity.Has<RigidBodyComponent>());
    CHECK(entity.Has<ColliderComponent>());
    CHECK(entity.Has<PhysicsMaterialComponent>());
    CHECK(entity.Get<ColliderComponent>().shape == ShapeType::Box);
    CHECK(entity.Get<ColliderComponent>().shape_dirty);
    CHECK(entity.Get<RigidBodyComponent>().type == BodyType::Dynamic);
}
