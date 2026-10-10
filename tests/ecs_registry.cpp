#include "ecs/component_registry.hpp"
#include "ecs/components/cfd.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/render.hpp"
#include "ecs/components/robot.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/components/serialize.hpp"
#include "logic/Simulation.h"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>

using namespace openphysx;

namespace {

void fill_distinct(Entity entity)
{
    entity.Get<TagComponent>().name = "Link";
    entity.Get<TransformComponent>().position = {1.0f, 2.0f, 3.0f};
    entity.Get<TransformComponent>().rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    entity.Get<TransformComponent>().scale = {2.0f, 3.0f, 4.0f};
    entity.Get<EditorStateComponent>().visible = false;
    entity.Get<EditorStateComponent>().locked = true;

    PrimitiveBoxComponent box;
    box.size = {0.5f, 0.25f, 0.125f};
    box.color = {0.1f, 0.2f, 0.3f};
    entity.Add<PrimitiveBoxComponent>(box);

    MeshRendererComponent mesh;
    mesh.meshAsset = 0x1111111111111111ull;
    mesh.materialAsset = 0x2222222222222222ull;
    mesh.castShadow = false;
    entity.Add<MeshRendererComponent>(mesh);

    MaterialOverrideComponent material;
    material.albedo = {0.4f, 0.5f, 0.6f};
    material.roughness = 0.2f;
    material.metallic = 0.8f;
    entity.Add<MaterialOverrideComponent>(material);

    RigidBodyComponent body;
    body.type = BodyType::Kinematic;
    body.mass = 2.5f;
    body.inertia.m[0] = 4.0f;
    body.autoInertia = false;
    body.linearVelocity = {0.1f, 0.2f, 0.3f};
    body.angularVelocity = {0.4f, 0.5f, 0.6f};
    body.linearDamping = 0.7f;
    body.angularDamping = 0.8f;
    body.gravityEnabled = false;
    body.ccd = true;
    entity.Add<RigidBodyComponent>(body);

    ColliderComponent collider;
    collider.shape = ShapeType::Capsule;
    collider.halfExtents = {0.2f, 0.3f, 0.4f};
    collider.radius = 0.15f;
    collider.height = 1.25f;
    collider.offset = {0.0f, 0.5f, 0.0f};
    collider.physicsMaterial = 0x3333333333333333ull;
    collider.layer = 3;
    collider.mask = 9;
    collider.isSensor = true;
    entity.Add<ColliderComponent>(collider);

    PhysicsMaterialComponent friction;
    friction.friction = 0.35f;
    friction.restitution = 0.15f;
    friction.rollingFriction = 0.05f;
    entity.Add<PhysicsMaterialComponent>(friction);

    JointComponent joint;
    joint.type = JointType::Revolute;
    joint.bodyA = 0x4444444444444444ull;
    joint.bodyB = 0x5555555555555555ull;
    joint.anchorA = {1.0f, 0.0f, 0.0f};
    joint.anchorB = {0.0f, 1.0f, 0.0f};
    joint.axisA = {0.0f, 0.0f, 1.0f};
    joint.axisB = {1.0f, 0.0f, 0.0f};
    joint.limitMin = -1.5f;
    joint.limitMax = 1.5f;
    joint.limitEnabled = true;
    joint.motorTarget = 0.25f;
    joint.motorMaxForce = 12.0f;
    joint.motorEnabled = true;
    entity.Add<JointComponent>(joint);

    RobotComponent robot;
    robot.name = "Arm";
    robot.rootLink = 0x6666666666666666ull;
    entity.Add<RobotComponent>(robot);

    LinkComponent link;
    link.robot = 0x7777777777777777ull;
    link.index = 3;
    entity.Add<LinkComponent>(link);

    ActuatorComponent actuator;
    actuator.type = ActuatorType::Prismatic;
    actuator.joint = 0x8888888888888888ull;
    actuator.effortLimit = 40.0f;
    actuator.velocityLimit = 1.5f;
    entity.Add<ActuatorComponent>(actuator);

    SensorComponent sensor;
    sensor.type = SensorType::Imu;
    sensor.link = 0x9999999999999999ull;
    sensor.frame = "wrist";
    entity.Add<SensorComponent>(sensor);

    CfdDomainComponent domain;
    domain.boundsMin = {-2.0f, -3.0f, -4.0f};
    domain.boundsMax = {2.0f, 3.0f, 4.0f};
    domain.resolutionX = 8;
    domain.resolutionY = 10;
    domain.resolutionZ = 12;
    domain.density = 1.1f;
    domain.viscosity = 0.002f;
    domain.turbulence = TurbulenceModel::KEpsilon;
    domain.boundaries.push_back(BoundaryCondition{BoundaryKind::Inlet, {1.0f, 0.0f, 0.0f}, 101325.0f});
    entity.Add<CfdDomainComponent>(domain);

    entity.Add<SelectionOutlineTag>(SelectionOutlineTag{false});
    entity.Add<MeshGpuHandle>(MeshGpuHandle{7, 8});
    entity.Add<PhysicsBodyHandle>(PhysicsBodyHandle{4});
    entity.Add<ContactCache>(ContactCache{6});
    entity.Add<CfdResultField>(CfdResultField{64, 1.25f});
}

} // namespace

TEST_CASE("registry serializes every saved component and ignores unknown fields")
{
    EnsureComponentsRegistered();
    const ComponentRegistry& registry = ComponentRegistry::Instance();
    int saved = 0;
    int runtime = 0;
    for (const ComponentInfo& info : registry.All())
    {
        CHECK(info.serialize);
        CHECK(info.deserialize);
        CHECK(info.copy);
        CHECK(info.equals);
        CHECK(info.remove);
        if (!info.serializable)
        {
            ++runtime;
            continue;
        }
        ++saved;
        Scene scene;
        Entity entity = scene.CreateEntity("Round");
        LoadReport report;
        nlohmann::json probe = info.serialize(entity);
        if (!info.has(entity))
        {
            info.deserialize(entity, nlohmann::json::object(), report);
            probe = info.serialize(entity);
        }
        probe["notAField"] = "ignored";
        Scene again;
        Entity destination = again.CreateEntityWithUUID(entity.GetUUID(), "Round");
        info.deserialize(destination, probe, report);
        CHECK(report.entries.empty());
        CHECK(info.serialize(entity) == info.serialize(destination));
    }
    CHECK(saved == 20);
    CHECK(runtime == 5);

    TagComponent tag;
    const nlohmann::json partial = {{"unknown", 42}};
    from_json(partial, tag);
    CHECK(tag.name == "Entity");
}

TEST_CASE("scene copy keeps saved components and drops runtime handles")
{
    Scene scene;
    Entity entity = scene.CreateEntityWithUUID(42, "Cube");
    fill_distinct(entity);
    Entity child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, entity, false));

    const Scene copy = scene;
    CHECK(copy == scene);
    const Entity copied = copy.FindByUUID(42);
    REQUIRE(copied);
    CHECK(copied.Get<TagComponent>().name == "Link");
    CHECK(copied.Get<RigidBodyComponent>().type == BodyType::Kinematic);
    CHECK(copied.Get<RigidBodyComponent>().mass == doctest::Approx(2.5f));
    CHECK(copied.Get<JointComponent>().bodyA == 0x4444444444444444ull);
    CHECK(copied.Get<JointComponent>().type == JointType::Revolute);
    CHECK(copied.Get<CfdDomainComponent>().resolutionY == 10);
    CHECK(copied.Get<CfdDomainComponent>().boundaries.size() == 1);
    CHECK(copied.Has<SelectionOutlineTag>());
    CHECK_FALSE(copied.Get<SelectionOutlineTag>().marked);
    CHECK_FALSE(copied.Has<MeshGpuHandle>());
    CHECK_FALSE(copied.Has<PhysicsBodyHandle>());
    CHECK_FALSE(copied.Has<ContactCache>());
    CHECK_FALSE(copied.Has<CfdResultField>());
    CHECK(copy.FindByUUID(child.GetUUID()).Get<RelationshipComponent>().parent == 42);
}

TEST_CASE("play copies the editor scene and stop discards the copy")
{
    Simulation simulation;
    const UUID id = simulation.active_id();
    REQUIRE(simulation.editor_scene().FindByUUID(id));
    CHECK(simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position.y == doctest::Approx(1.0f));

    simulation.play();
    CHECK(simulation.simulating());
    CHECK(simulation.state().playing);
    simulation.scene().FindByUUID(id).Get<TransformComponent>().position = {4.0f, 5.0f, 6.0f};
    CHECK(simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position.y == doctest::Approx(1.0f));

    simulation.pause();
    CHECK_FALSE(simulation.state().playing);
    CHECK(simulation.simulating());
    CHECK(simulation.scene().FindByUUID(id).Get<TransformComponent>().position.x == doctest::Approx(4.0f));

    simulation.play();
    CHECK(simulation.scene().FindByUUID(id).Get<TransformComponent>().position.x == doctest::Approx(4.0f));

    simulation.stop();
    CHECK_FALSE(simulation.simulating());
    CHECK_FALSE(simulation.state().playing);
    CHECK(simulation.scene().FindByUUID(id).Get<TransformComponent>().position.y == doctest::Approx(1.0f));
    CHECK(simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position ==
          simulation.scene().FindByUUID(id).Get<TransformComponent>().position);
}

TEST_CASE("float values survive a component json round trip")
{
    const float samples[] = {
        0.0f,
        -0.0f,
        1.0f,
        -1.0f,
        0.1f,
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::epsilon(),
        1.8e-5f,
        101325.0f,
    };
    for (const float sample : samples)
    {
        TransformComponent transform;
        transform.position = {sample, -sample, sample * 0.5f};
        const nlohmann::json json = transform;
        const TransformComponent loaded = json.get<TransformComponent>();
        CHECK(loaded.position.x == sample);
        CHECK(loaded.position.y == -sample);
        CHECK(loaded.position.z == sample * 0.5f);
    }
}
