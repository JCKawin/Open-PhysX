#include "core/Math.h"
#include "core/View.h"
#include "ecs/pick.hpp"
#include "ecs/scene.hpp"
#include "ecs/transform_ops.hpp"
#include "persistence/scene_serializer.hpp"

#include <doctest/doctest.h>

using namespace openphysx;

namespace {

Entity make_box(Scene& scene, const char* name, Vec3 position)
{
    Entity entity = scene.CreateEntity(name);
    entity.Get<TransformComponent>().position = position;
    entity.Add<PrimitiveBoxComponent>();
    return entity;
}

} // namespace

TEST_CASE("cursor edits do not change object transforms")
{
    Scene scene;
    Entity cube = make_box(scene, "Cube", {0.0f, 1.0f, 0.0f});
    const TransformComponent before = cube.Get<TransformComponent>();
    scene.cursor().position = {3.0f, 0.0f, -1.0f};
    scene.cursor().rotation = quat_axis_angle({0.0f, 1.0f, 0.0f}, 0.4f);
    scene.cursor().visible = false;
    scene.cursor().snap = true;
    CHECK(cube.Get<TransformComponent>() == before);
    CHECK(scene.GetWorldTransform(cube).position.y == doctest::Approx(1.0f));
}

TEST_CASE("toggling cursor visibility leaves objects alone")
{
    Scene scene;
    Entity cube = make_box(scene, "Cube", {2.0f, 1.0f, 0.0f});
    const TransformComponent before = cube.Get<TransformComponent>();
    scene.cursor().visible = false;
    scene.cursor().visible = true;
    CHECK(cube.Get<TransformComponent>() == before);
}

TEST_CASE("rotating around the 3D cursor orbits the object and leaves the cursor")
{
    Scene scene;
    Entity cube = make_box(scene, "Cube", {2.0f, 0.0f, 0.0f});
    scene.cursor().position = {0.0f, 0.0f, 0.0f};
    const Vec3 cursor_before = scene.cursor().position;
    const TransformComponent world = scene.GetWorldTransform(cube);
    const Quat quarter = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    scene.SetWorldTransform(cube, RotateWorld(world, scene.cursor().position, quarter));
    const TransformComponent after = scene.GetWorldTransform(cube);
    CHECK(after.position.x == doctest::Approx(0.0f).epsilon(1.0e-4));
    CHECK(after.position.z == doctest::Approx(-2.0f).epsilon(1.0e-4));
    CHECK(scene.cursor().position.x == doctest::Approx(cursor_before.x));
    CHECK(scene.cursor().position.z == doctest::Approx(cursor_before.z));
}

TEST_CASE("individual origins rotate each object about its own position")
{
    TransformComponent a;
    a.position = {2.0f, 0.0f, 0.0f};
    TransformComponent b;
    b.position = {-2.0f, 0.0f, 0.0f};
    const Quat quarter = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    const TransformComponent a2 = RotateWorld(a, a.position, quarter);
    const TransformComponent b2 = RotateWorld(b, b.position, quarter);
    CHECK(a2.position.x == doctest::Approx(2.0f));
    CHECK(a2.position.z == doctest::Approx(0.0f));
    CHECK(b2.position.x == doctest::Approx(-2.0f));
    CHECK(b2.position.z == doctest::Approx(0.0f));
}

TEST_CASE("a front-view ray with no box misses the ground plane")
{
    View3D view{};
    view.dist = 10.0f;
    view_set_axis(view, ViewAxis::Front);
    view_set_pivot(view, {0.0f, 1.0f, 0.0f});
    Vec3 hit{};
    CHECK_FALSE(PlaceCursorOnRay(view, 0.0f, 0.0f, 1.0f, {}, hit));
}

TEST_CASE("a top-view ray hits the ground at the origin")
{
    View3D view{};
    view.dist = 10.0f;
    view_set_axis(view, ViewAxis::Top);
    view_set_pivot(view, {0.0f, 1.0f, 0.0f});
    Vec3 hit{};
    REQUIRE(PlaceCursorOnRay(view, 0.0f, 0.0f, 1.0f, {}, hit));
    CHECK(hit.x == doctest::Approx(0.0f).epsilon(1.0e-3));
    CHECK(hit.y == doctest::Approx(0.0f).epsilon(1.0e-3));
    CHECK(hit.z == doctest::Approx(0.0f).epsilon(1.0e-3));
}

TEST_CASE("a ray hits a box before the ground")
{
    View3D view{};
    view.dist = 10.0f;
    view_set_axis(view, ViewAxis::Top);
    view_set_pivot(view, {0.0f, 4.0f, 0.0f});
    RigidBody box;
    box.position = {0.0f, 1.0f, 0.0f};
    box.size = {2.0f, 2.0f, 2.0f};
    Vec3 hit{};
    REQUIRE(PlaceCursorOnRay(view, 0.0f, 0.0f, 1.0f, {box}, hit));
    CHECK(hit.y == doctest::Approx(2.0f).epsilon(1.0e-3));
}

TEST_CASE("snap rounds each axis to the grid")
{
    const Vec3 snapped = SnapPosition({1.24f, -0.6f, 3.9f}, 1.0f);
    CHECK(snapped.x == doctest::Approx(1.0f));
    CHECK(snapped.y == doctest::Approx(-1.0f));
    CHECK(snapped.z == doctest::Approx(4.0f));
}

TEST_CASE("cursor survives save, load, copy, and a missing file key")
{
    Scene scene;
    scene.cursor().position = {1.5f, 0.0f, -2.0f};
    scene.cursor().visible = false;
    scene.cursor().snap = true;
    Scene loaded;
    LoadReport report;
    REQUIRE(ReadScene(WriteScene(scene), loaded, report));
    CHECK(loaded.cursor() == scene.cursor());

    Scene copy = scene;
    CHECK(copy.cursor().position.x == doctest::Approx(1.5f));
    CHECK(copy.cursor().visible == false);

    Scene from_old;
    REQUIRE(ReadScene(nlohmann::json{{"version", 2}, {"entities", nlohmann::json::array()}}, from_old, report));
    CHECK(from_old.cursor().visible);
    CHECK(from_old.cursor().position.x == doctest::Approx(0.0f));
}

TEST_CASE("reset cursor restores defaults without touching objects")
{
    Scene scene;
    Entity cube = make_box(scene, "Cube", {4.0f, 1.0f, 0.0f});
    scene.cursor().position = {9.0f, 2.0f, 1.0f};
    scene.cursor().visible = false;
    scene.cursor() = {};
    CHECK(scene.cursor().position.x == doctest::Approx(0.0f));
    CHECK(scene.cursor().visible);
    CHECK(cube.Get<TransformComponent>().position.x == doctest::Approx(4.0f));
}
