#include "core/View.h"
#include "ecs/pick.hpp"
#include "ecs/transform_ops.hpp"

#include <doctest/doctest.h>

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
