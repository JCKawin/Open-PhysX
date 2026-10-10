#include "ecs/components/cfd.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/render.hpp"
#include "ecs/components/robot.hpp"
#include "ecs/components/slots.hpp"
#include "persistence/scene_serializer.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <limits>

using namespace openphysx;

namespace {

std::string canonical(const nlohmann::json& json)
{
    return json.dump(2);
}

Entity add_filled(Scene& scene, UUID id, std::string name)
{
    Entity entity = scene.CreateEntityWithUUID(id, std::move(name));
    entity.Get<TransformComponent>().position = {0.1f, -0.0f, std::numeric_limits<float>::epsilon()};
    entity.Get<TransformComponent>().scale = {std::numeric_limits<float>::min(), 1.0f, std::numeric_limits<float>::max()};

    PrimitiveBoxComponent box;
    box.size = {0.1f, 2.0f, 3.0f};
    box.color = {0.15f, 0.35f, 0.85f};
    entity.Add<PrimitiveBoxComponent>(box);

    MeshRendererComponent mesh;
    mesh.meshAsset = 0x1111111111111111ull;
    mesh.materialAsset = 0x2222222222222222ull;
    entity.Add<MeshRendererComponent>(mesh);

    MaterialOverrideComponent material;
    material.metallic = 0.1f;
    entity.Add<MaterialOverrideComponent>(material);

    RigidBodyComponent body;
    body.type = BodyType::Static;
    body.mass = 0.1f;
    body.inertia.m[0] = std::numeric_limits<float>::min();
    body.inertia.m[4] = 0.1f;
    body.inertia.m[8] = std::numeric_limits<float>::max();
    body.autoInertia = false;
    entity.Add<RigidBodyComponent>(body);

    ColliderComponent collider;
    collider.shape = ShapeType::Sphere;
    collider.radius = 0.1f;
    collider.physicsMaterial = id;
    entity.Add<ColliderComponent>(collider);

    entity.Add<PhysicsMaterialComponent>(PhysicsMaterialComponent{0.1f, 0.2f, 0.3f});

    RobotComponent robot;
    robot.name = "Arm";
    robot.rootLink = id;
    entity.Add<RobotComponent>(robot);

    LinkComponent link;
    link.robot = id;
    link.index = 2;
    entity.Add<LinkComponent>(link);

    ActuatorComponent actuator;
    actuator.type = ActuatorType::Prismatic;
    actuator.joint = id;
    entity.Add<ActuatorComponent>(actuator);

    SensorComponent sensor;
    sensor.type = SensorType::Force;
    sensor.link = id;
    sensor.frame = "tip";
    entity.Add<SensorComponent>(sensor);

    CfdDomainComponent domain;
    domain.density = 0.1f;
    domain.viscosity = 1.8e-5f;
    domain.turbulence = TurbulenceModel::None;
    domain.boundaries.push_back(BoundaryCondition{BoundaryKind::Outlet, {0.1f, 0.0f, 0.0f}, 0.1f});
    entity.Add<CfdDomainComponent>(domain);

    entity.Get<FluidRoleComponent>().role = FluidRole::Flow;
    return entity;
}

} // namespace

TEST_CASE("scene json round trip is byte identical and keeps every saved component")
{
    Scene scene;
    Entity entity = add_filled(scene, 0x10ull, "Base");
    Entity child = scene.CreateEntityWithUUID(0x11ull, "Child");
    REQUIRE(scene.SetParent(child, entity, false));

    JointComponent joint;
    joint.type = JointType::Revolute;
    joint.bodyA = 0x10ull;
    joint.bodyB = 0x11ull;
    joint.motorMaxForce = 0.1f;
    child.Add<JointComponent>(joint);

    const nlohmann::json written = WriteScene(scene);
    const std::string first = canonical(written);

    Scene loaded;
    LoadReport report;
    const auto result = ReadScene(written, loaded, report);
    REQUIRE(result.has_value());
    CHECK(report.entries.empty());
    CHECK(loaded == scene);
    CHECK(canonical(WriteScene(loaded)) == first);

    const Entity again = loaded.FindByUUID(0x10ull);
    REQUIRE(again);
    CHECK(std::signbit(again.Get<TransformComponent>().position.y));
    CHECK(again.Get<TransformComponent>().position.x == 0.1f);
    CHECK(again.Get<TransformComponent>().scale.x == std::numeric_limits<float>::min());
    CHECK(again.Get<TransformComponent>().scale.z == std::numeric_limits<float>::max());
    CHECK(again.Get<RigidBodyComponent>().inertia.m[0] == std::numeric_limits<float>::min());
    CHECK(again.Get<RigidBodyComponent>().inertia.m[4] == 0.1f);
    CHECK(again.Get<RigidBodyComponent>().inertia.m[8] == std::numeric_limits<float>::max());
    CHECK(again.Get<RigidBodyComponent>().mass == 0.1f);
    CHECK(loaded.FindByUUID(0x11ull).Get<JointComponent>().bodyA == 0x10ull);
    CHECK(loaded.FindByUUID(0x11ull).Get<JointComponent>().bodyB == 0x11ull);
}

TEST_CASE("joints keep their targets when the joint is stored before the bodies")
{
    Scene scene;
    Entity joint_entity = scene.CreateEntityWithUUID(0x20ull, "Hinge");
    JointComponent joint;
    joint.type = JointType::Spherical;
    joint.bodyA = 0x21ull;
    joint.bodyB = 0x22ull;
    joint_entity.Add<JointComponent>(joint);
    scene.CreateEntityWithUUID(0x21ull, "A");
    scene.CreateEntityWithUUID(0x22ull, "B");

    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(WriteScene(scene), loaded, report));
    CHECK(report.entries.empty());
    const JointComponent& loaded_joint = loaded.FindByUUID(0x20ull).Get<JointComponent>();
    CHECK(loaded_joint.type == JointType::Spherical);
    CHECK(loaded_joint.bodyA == 0x21ull);
    CHECK(loaded_joint.bodyB == 0x22ull);
    CHECK(loaded.CreationOrder().front() == 0x20ull);
}

TEST_CASE("unknown components and dangling references are reported")
{
    nlohmann::json json = {
        {"entities",
         nlohmann::json::array({
             nlohmann::json{
                 {"id", "00000000000000aa"},
                 {"components",
                  {{"Tag", {{"name", "Loose"}, {"extra", true}}},
                   {"Joint", {{"type", "Fixed"}, {"bodyA", "00000000000000bb"}, {"bodyB", "0000000000000000"}}},
                   {"FutureWidget", {{"answer", 7}}}}},
             },
         })},
    };

    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(json, loaded, report));
    CHECK(loaded.FindByUUID(0xaaull).Get<TagComponent>().name == "Loose");
    CHECK(loaded.FindByUUID(0xaaull).Get<JointComponent>().bodyA == kNullUuid);
    CHECK_FALSE(loaded.UnknownComponents().empty());

    bool saw_unknown = false;
    bool saw_dangling = false;
    for (const LoadReport::Entry& entry : report.entries)
    {
        if (entry.message.find("FutureWidget") != std::string::npos)
            saw_unknown = true;
        if (entry.message.find("bodyA") != std::string::npos)
            saw_dangling = true;
    }
    CHECK(saw_unknown);
    CHECK(saw_dangling);

    Scene again;
    LoadReport second;
    const nlohmann::json rewritten = WriteScene(loaded);
    REQUIRE(ReadScene(rewritten, again, second));
    CHECK(again.UnknownComponents().at(0xaaull).at(0).name == "FutureWidget");
    CHECK(rewritten.at("entities").at(0).at("components").contains("FutureWidget"));
}

TEST_CASE("a scene without an entities array is refused and the previous scene stays")
{
    Scene scene;
    const UUID id = scene.CreateEntity("Keep").GetUUID();
    LoadReport report;
    const auto result = ReadScene(nlohmann::json::object(), scene, report);
    CHECK_FALSE(result.has_value());
    CHECK(result.error().kind == LoadError::Kind::Schema);
    CHECK(scene.FindByUUID(id));
}

TEST_CASE("the scene version is written, a newer version is refused, and an unversioned file opens")
{
    Scene scene;
    scene.CreateEntity("Cube");
    CHECK(WriteScene(scene).at("version") == 1);

    LoadReport report;
    const nlohmann::json newer = {{"version", 2}, {"entities", nlohmann::json::array()}};
    const auto refused = ReadScene(newer, scene, report);
    CHECK_FALSE(refused.has_value());
    CHECK(refused.error().kind == LoadError::Kind::TooNew);
    CHECK(scene.FindByName("Cube"));

    const nlohmann::json unversioned = {{"entities", nlohmann::json::array()}};
    CHECK(ReadScene(unversioned, scene, report).has_value());
}

TEST_CASE("empty slots survive a round trip with their values")
{
    Scene scene;
    Entity cube = scene.CreateEntity("Cube");
    cube.Get<FluidRoleComponent>().role = FluidRole::Effector;
    cube.Get<MeshRendererComponent>().meshAsset = 0x77ull;

    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(WriteScene(scene), loaded, report));
    CHECK(report.entries.empty());
    const Entity again = loaded.FindByName("Cube");
    REQUIRE(again);
    CHECK(again.Get<FluidRoleComponent>().role == FluidRole::Effector);
    CHECK(again.Get<MeshRendererComponent>().meshAsset == 0x77ull);
    CHECK(again.Get<ConstraintStackComponent>().entries.empty());
    CHECK(again.Get<ModifierStackComponent>().entries.empty());
    CHECK(loaded == scene);
}
