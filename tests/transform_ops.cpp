#include "core/View.h"
#include "ecs/pick.hpp"
#include "ecs/scene.hpp"
#include "ecs/transform_ops.hpp"

#include <doctest/doctest.h>

#include <cmath>

using namespace openphysx;

namespace {

TransformComponent at(Vec3 position)
{
    TransformComponent world;
    world.position = position;
    return world;
}

} // namespace

TEST_CASE("move, rotate and scale act on a world transform about a pivot")
{
    const TransformComponent moved = MoveWorld(at({1.0f, 0.0f, 0.0f}), {0.0f, 2.0f, 0.0f});
    CHECK(moved.position.x == doctest::Approx(1.0f));
    CHECK(moved.position.y == doctest::Approx(2.0f));

    // A quarter turn about +Y takes +X to -Z.
    const Quat quarter = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    const TransformComponent turned = RotateWorld(at({1.0f, 0.0f, 0.0f}), {0.0f, 0.0f, 0.0f}, quarter);
    CHECK(turned.position.x == doctest::Approx(0.0f).epsilon(1.0e-4));
    CHECK(turned.position.z == doctest::Approx(-1.0f).epsilon(1.0e-4));

    // The pivot stays put and the object keeps its distance from it.
    const TransformComponent spun = RotateWorld(at({3.0f, 0.0f, 0.0f}), {2.0f, 0.0f, 0.0f}, quarter);
    CHECK(spun.position.x == doctest::Approx(2.0f).epsilon(1.0e-4));
    CHECK(spun.position.z == doctest::Approx(-1.0f).epsilon(1.0e-4));

    const TransformComponent grown = ScaleWorld(at({3.0f, 0.0f, 0.0f}), {1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}, 0.05f);
    CHECK(grown.position.x == doctest::Approx(5.0f));
    CHECK(grown.scale.x == doctest::Approx(2.0f));

    const TransformComponent shrunk = ScaleWorld(at({0.0f, 0.0f, 0.0f}), {0.0f, 0.0f, 0.0f}, {0.001f, 1.0f, 1.0f}, 0.05f);
    CHECK(shrunk.scale.x == doctest::Approx(0.05f));
}

TEST_CASE("several objects rotate and scale about their median position")
{
    const Vec3 middle = MeanPosition({at({0.0f, 0.0f, 0.0f}), at({2.0f, 0.0f, 0.0f}), at({4.0f, 0.0f, 0.0f})});
    CHECK(middle.x == doctest::Approx(2.0f));
    CHECK(MeanPosition({}).x == 0.0f);
}

TEST_CASE("parent at (10,0,0) with child local (0,2,0) composes to (10,2,0)")
{
    TransformComponent parent;
    parent.position = {10.0f, 0.0f, 0.0f};
    TransformComponent local;
    local.position = {0.0f, 2.0f, 0.0f};
    const TransformComponent world = ComposeLocalIntoParent(parent, local);
    CHECK(world.position.x == doctest::Approx(10.0f));
    CHECK(world.position.y == doctest::Approx(2.0f));
    CHECK(world.position.z == doctest::Approx(0.0f));
}

TEST_CASE("a rotated parent turns a local translation in world")
{
    TransformComponent parent;
    parent.rotation = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    TransformComponent local;
    local.position = {1.0f, 0.0f, 0.0f};
    const TransformComponent world = ComposeLocalIntoParent(parent, local);
    CHECK(world.position.x == doctest::Approx(0.0f).epsilon(1.0e-4));
    CHECK(world.position.z == doctest::Approx(-1.0f).epsilon(1.0e-4));
}

TEST_CASE("nonuniform parent scale stretches local translation on those axes")
{
    TransformComponent parent;
    parent.scale = {2.0f, 1.0f, 1.0f};
    TransformComponent local;
    local.position = {1.0f, 2.0f, 0.0f};
    const TransformComponent world = ComposeLocalIntoParent(parent, local);
    CHECK(world.position.x == doctest::Approx(2.0f));
    CHECK(world.position.y == doctest::Approx(2.0f));
}

TEST_CASE("compose then uncompose round-trips an invertible parent")
{
    TransformComponent parent;
    parent.position = {3.0f, -1.0f, 4.0f};
    parent.rotation = quat_axis_angle({0.0f, 0.0f, 1.0f}, 0.4f);
    parent.scale = {2.0f, 0.5f, 1.5f};
    TransformComponent local;
    local.position = {0.25f, 1.0f, -0.5f};
    local.rotation = quat_axis_angle({1.0f, 0.0f, 0.0f}, 0.3f);
    local.scale = {1.2f, 0.8f, 1.0f};
    const TransformComponent world = ComposeLocalIntoParent(parent, local);
    const UncomposeResult back = UncomposeWorldFromParent(parent, world);
    CHECK(back.invertible);
    CHECK(back.local.position.x == doctest::Approx(local.position.x).epsilon(1.0e-4));
    CHECK(back.local.position.y == doctest::Approx(local.position.y).epsilon(1.0e-4));
    CHECK(back.local.position.z == doctest::Approx(local.position.z).epsilon(1.0e-4));
    CHECK(back.local.scale.x == doctest::Approx(local.scale.x).epsilon(1.0e-4));
    CHECK(back.local.scale.y == doctest::Approx(local.scale.y).epsilon(1.0e-4));
    CHECK(back.local.rotation.x == doctest::Approx(local.rotation.x).epsilon(1.0e-4));
    CHECK(back.local.rotation.w == doctest::Approx(local.rotation.w).epsilon(1.0e-4));
}

TEST_CASE("a zero parent scale reports singular and stays finite")
{
    TransformComponent parent;
    parent.scale = {0.0f, 1.0f, 1.0f};
    TransformComponent world;
    world.position = {4.0f, 1.0f, 0.0f};
    world.scale = {2.0f, 1.0f, 1.0f};
    const UncomposeResult back = UncomposeWorldFromParent(parent, world);
    CHECK_FALSE(back.invertible);
    CHECK_FALSE(std::isnan(back.local.position.x));
    CHECK_FALSE(std::isnan(back.local.scale.x));
    CHECK(back.local.position.x == doctest::Approx(0.0f));
}

TEST_CASE("local move follows the object axes; world move follows world axes")
{
    TransformComponent pose;
    pose.rotation = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    const TransformComponent local_step = MoveLocal(pose, {1.0f, 0.0f, 0.0f});
    CHECK(local_step.position.x == doctest::Approx(0.0f).epsilon(1.0e-4));
    CHECK(local_step.position.z == doctest::Approx(-1.0f).epsilon(1.0e-4));
    const TransformComponent world_step = MoveWorld(pose, {1.0f, 0.0f, 0.0f});
    CHECK(world_step.position.x == doctest::Approx(1.0f));
    CHECK(world_step.position.z == doctest::Approx(0.0f));
}

TEST_CASE("XYZ euler degrees round-trip a quaternion")
{
    const Quat source = quat_from_euler_xyz({deg_to_rad(20.0f), deg_to_rad(-35.0f), deg_to_rad(80.0f)});
    const Vec3 euler = quat_to_euler_xyz(source);
    const Quat again = quat_from_euler_xyz(euler);
    const Vec3 x = quat_rotate(source, {1.0f, 0.0f, 0.0f});
    const Vec3 x2 = quat_rotate(again, {1.0f, 0.0f, 0.0f});
    CHECK(x2.x == doctest::Approx(x.x).epsilon(1.0e-4));
    CHECK(x2.y == doctest::Approx(x.y).epsilon(1.0e-4));
    CHECK(x2.z == doctest::Approx(x.z).epsilon(1.0e-4));
}

TEST_CASE("local rotation post-multiplies; world rotation pre-multiplies")
{
    TransformComponent pose;
    pose.rotation = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    const Quat pitch = quat_axis_angle({1.0f, 0.0f, 0.0f}, kPi * 0.5f);
    const TransformComponent local = RotateLocal(pose, pitch);
    const Vec3 local_y = quat_rotate(local.rotation, {0.0f, 1.0f, 0.0f});
    CHECK(local_y.x == doctest::Approx(1.0f).epsilon(1.0e-4));
    const TransformComponent world = RotateWorld(pose, {}, pitch);
    const Vec3 world_y = quat_rotate(world.rotation, {0.0f, 1.0f, 0.0f});
    CHECK(world_y.z == doctest::Approx(1.0f).epsilon(1.0e-4));
}

TEST_CASE("a box is picked where it is drawn and the nearest box wins")
{
    View3D view{};
    view.dist = 10.0f;
    view_set_axis(view, ViewAxis::Front);
    view_set_pivot(view, {0.0f, 0.0f, 0.0f});

    RigidBody near_box;
    near_box.position = {0.0f, 0.0f, 0.0f};
    near_box.size = {2.0f, 2.0f, 2.0f};
    RigidBody far_box;
    far_box.position = {0.0f, 0.0f, -3.0f};
    far_box.size = {2.0f, 2.0f, 2.0f};

    ScreenBox near_rect;
    ScreenBox far_rect;
    REQUIRE(ProjectBox(near_box, view, 800.0f, 600.0f, near_rect));
    REQUIRE(ProjectBox(far_box, view, 800.0f, 600.0f, far_rect));
    CHECK(BoxContains(near_rect, 400.0f, 300.0f));
    CHECK_FALSE(BoxContains(near_rect, 5.0f, 5.0f));
    CHECK(near_rect.depth < far_rect.depth);

    CHECK(BoxOverlaps(near_rect, 0.0f, 0.0f, 800.0f, 600.0f));
    CHECK_FALSE(BoxOverlaps(near_rect, 0.0f, 0.0f, 10.0f, 10.0f));
}

TEST_CASE("a box behind the camera is not projected")
{
    View3D view{};
    view.dist = 10.0f;
    view_set_axis(view, ViewAxis::Front);
    view_set_pivot(view, {0.0f, 0.0f, 0.0f});

    RigidBody behind;
    behind.position = {0.0f, 0.0f, 20.0f};
    behind.size = {1.0f, 1.0f, 1.0f};
    ScreenBox rect;
    CHECK_FALSE(ProjectBox(behind, view, 800.0f, 600.0f, rect));
}
