#include "ecs/components/cfd.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/render.hpp"
#include "persistence/scene_serializer.hpp"

#include <doctest/doctest.h>

#include <limits>

using namespace openphysx;

namespace {

const double kInf = std::numeric_limits<double>::infinity();

nlohmann::json entity_doc(nlohmann::json components)
{
    return nlohmann::json{
        {"entities",
         nlohmann::json::array({nlohmann::json{{"id", "00000000000000aa"}, {"components", std::move(components)}}})},
    };
}

bool has_warning(const LoadReport& report, const std::string& needle)
{
    for (const LoadReport::Entry& entry : report.entries)
    {
        if (entry.message.find(needle) != std::string::npos)
            return true;
    }
    return false;
}

} // namespace

TEST_CASE("non finite and non physical values are reset with load warnings")
{
    Scene scene;
    LoadReport report;
    const nlohmann::json doc = entity_doc({
        {"Tag", {{"name", "Bad"}}},
        {"Transform",
         {{"position", nlohmann::json::array({kInf, 1.0, 2.0})},
          {"rotation", nlohmann::json::array({0.0, 0.0, 0.0, 0.0})},
          {"scale", nlohmann::json::array({1.0, 1.0, 1.0})}}},
        {"PrimitiveBox", {{"size", nlohmann::json::array({0.0, -1.0, 3.0})}}},
        {"RigidBody",
         {{"type", "Dynamic"},
          {"mass", 0.0},
          {"inertia", nlohmann::json::array({kInf, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0})},
          {"linearVelocity", nlohmann::json::array({kInf, 0.0, 0.0})}}},
        {"Collider", {{"shape", "Sphere"}, {"radius", -3.0}}},
        {"PhysicsMaterial", {{"friction", kInf}}},
    });
    REQUIRE(ReadScene(doc, scene, report));

    const Entity entity = scene.FindByUUID(0xaaull);
    REQUIRE(entity);
    CHECK(entity.Get<TransformComponent>().position.x == 0.0f);
    CHECK(entity.Get<TransformComponent>().position.y == 1.0f);
    CHECK(entity.Get<TransformComponent>().rotation.w == 1.0f);
    CHECK(entity.Get<PrimitiveBoxComponent>().size.x == 2.0f);
    CHECK(entity.Get<PrimitiveBoxComponent>().size.y == 2.0f);
    CHECK(entity.Get<PrimitiveBoxComponent>().size.z == 3.0f);
    CHECK(entity.Get<RigidBodyComponent>().mass == 1.0f);
    CHECK(entity.Get<RigidBodyComponent>().inertia.m[0] == 1.0f);
    CHECK(entity.Get<RigidBodyComponent>().linearVelocity.x == 0.0f);
    CHECK(entity.Get<ColliderComponent>().radius == 0.5f);
    CHECK(entity.Get<PhysicsMaterialComponent>().friction == 0.5f);

    CHECK(has_warning(report, "position"));
    CHECK(has_warning(report, "rotation"));
    CHECK(has_warning(report, "box size"));
    CHECK(has_warning(report, "mass"));
    CHECK(has_warning(report, "inertia"));
    CHECK(has_warning(report, "linear velocity"));
    CHECK(has_warning(report, "collider radius"));
    CHECK(has_warning(report, "friction"));
}

TEST_CASE("box and capsule collider sizes are validated per shape")
{
    Scene scene;
    LoadReport report;
    const nlohmann::json doc = entity_doc({
        {"Tag", {{"name", "Shapes"}}},
        {"Collider",
         {{"shape", "Box"},
          {"halfExtents", nlohmann::json::array({0.0, 1.0, kInf})},
          {"height", 0.0},
          {"offset", nlohmann::json::array({0.0, kInf, 0.0})}}},
    });
    REQUIRE(ReadScene(doc, scene, report));

    const ColliderComponent& collider = scene.FindByUUID(0xaaull).Get<ColliderComponent>();
    CHECK(collider.halfExtents.x == 1.0f);
    CHECK(collider.halfExtents.y == 1.0f);
    CHECK(collider.halfExtents.z == 1.0f);
    CHECK(collider.offset.y == 0.0f);
    CHECK(has_warning(report, "collider half extents"));
    CHECK(has_warning(report, "collider offset"));
    // A box does not use radius or height, so those fields stay as loaded.
    CHECK_FALSE(has_warning(report, "collider height"));

    Scene capsules;
    LoadReport second;
    const nlohmann::json capsule_doc = entity_doc({
        {"Tag", {{"name", "Capsule"}}},
        {"Collider", {{"shape", "Capsule"}, {"radius", 0.0}, {"height", -2.0}}},
    });
    REQUIRE(ReadScene(capsule_doc, capsules, second));
    CHECK(capsules.FindByUUID(0xaaull).Get<ColliderComponent>().radius == 0.5f);
    CHECK(capsules.FindByUUID(0xaaull).Get<ColliderComponent>().height == 1.0f);
    CHECK(has_warning(second, "collider radius"));
    CHECK(has_warning(second, "collider height"));
}

TEST_CASE("joints and cfd domains are validated")
{
    Scene scene;
    LoadReport report;
    const nlohmann::json doc = entity_doc({
        {"Tag", {{"name", "Jointed"}}},
        {"Joint",
         {{"type", "Revolute"},
          {"anchorB", nlohmann::json::array({kInf, 0.0, 0.0})},
          {"axisA", nlohmann::json::array({0.0, 0.0, 0.0})},
          {"motorMaxForce", kInf}}},
        {"CfdDomain",
         {{"resolution", nlohmann::json::array({0, -4, 16})},
          {"density", 0.0},
          {"viscosity", kInf},
          {"boundaries",
           nlohmann::json::array(
               {{{"kind", "Inlet"}, {"velocity", nlohmann::json::array({0.0, 0.0, kInf})}, {"pressure", 0.0}}})}}},
    });
    REQUIRE(ReadScene(doc, scene, report));

    const Entity entity = scene.FindByUUID(0xaaull);
    REQUIRE(entity);
    CHECK(entity.Get<JointComponent>().anchorB.x == 0.0f);
    CHECK(entity.Get<JointComponent>().axisA.y == 1.0f);
    CHECK(entity.Get<JointComponent>().motorMaxForce == 0.0f);
    CHECK(entity.Get<CfdDomainComponent>().resolutionX == 32);
    CHECK(entity.Get<CfdDomainComponent>().resolutionY == 32);
    CHECK(entity.Get<CfdDomainComponent>().resolutionZ == 16);
    CHECK(entity.Get<CfdDomainComponent>().density == 1.2f);
    CHECK(entity.Get<CfdDomainComponent>().viscosity == doctest::Approx(1.8e-5f));
    CHECK(entity.Get<CfdDomainComponent>().boundaries.front().velocity.z == 0.0f);

    CHECK(has_warning(report, "joint anchor B"));
    CHECK(has_warning(report, "joint axis A"));
    CHECK(has_warning(report, "joint motor force"));
    CHECK(has_warning(report, "CFD resolution"));
    CHECK(has_warning(report, "CFD density"));
    CHECK(has_warning(report, "CFD viscosity"));
    CHECK(has_warning(report, "boundary velocity"));
}

TEST_CASE("a static or kinematic body keeps a zero mass so re-saving stays stable")
{
    Scene scene;
    LoadReport report;
    const nlohmann::json doc = {
        {"entities",
         nlohmann::json::array({
             nlohmann::json{
                 {"id", "00000000000000a1"},
                 {"components", {{"Tag", {{"name", "Stat"}}}, {"RigidBody", {{"type", "Static"}, {"mass", 0.0}}}}}},
             nlohmann::json{
                 {"id", "00000000000000a2"},
                 {"components", {{"Tag", {{"name", "Kin"}}}, {"RigidBody", {{"type", "Kinematic"}, {"mass", 0.0}}}}}},
         })},
    };
    REQUIRE(ReadScene(doc, scene, report));
    CHECK(scene.FindByUUID(0xa1ull).Get<RigidBodyComponent>().mass == 0.0f);
    CHECK(scene.FindByUUID(0xa2ull).Get<RigidBodyComponent>().mass == 0.0f);
    CHECK(report.entries.empty());
}

TEST_CASE("a sane scene loads without validation warnings")
{
    Scene scene;
    Entity entity = scene.CreateEntityWithUUID(0xbbull, "Good");
    entity.Get<TransformComponent>().position = {1.0f, 2.0f, 3.0f};
    RigidBodyComponent body;
    body.mass = 2.0f;
    entity.Add<RigidBodyComponent>(body);
    ColliderComponent collider;
    collider.shape = ShapeType::Capsule;
    collider.radius = 0.25f;
    collider.height = 1.5f;
    entity.Add<ColliderComponent>(collider);

    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(WriteScene(scene), loaded, report));
    CHECK(report.entries.empty());
    CHECK(loaded == scene);
}
