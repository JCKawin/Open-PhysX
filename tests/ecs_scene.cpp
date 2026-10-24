#include "core/Math.h"
#include "ecs/scene.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <vector>

using namespace openphysx;

TEST_CASE("create destroy and uuid lookup")
{
    Scene scene;
    const Entity first = scene.CreateEntity("Base");
    REQUIRE(first);
    CHECK(first.GetUUID() != kNullUuid);
    CHECK(scene.FindByUUID(first.GetUUID()).Get<TagComponent>().name == "Base");
    CHECK(scene.FindByName("Base").GetUUID() == first.GetUUID());
    CHECK(first.Has<IDComponent>());
    CHECK(first.Has<TagComponent>());
    CHECK(first.Has<TransformComponent>());
    CHECK(first.Has<RelationshipComponent>());
    CHECK(first.Has<EditorStateComponent>());

    const Entity duplicate = scene.CreateEntityWithUUID(first.GetUUID(), "Other");
    CHECK_FALSE(duplicate);
    CHECK(scene.CreationOrder().size() == 1);

    const Entity named = scene.CreateEntityWithUUID(0xA1B2C3D4E5F60718ull, "Link");
    REQUIRE(named);
    CHECK(named.GetUUID() == 0xA1B2C3D4E5F60718ull);
    CHECK_FALSE(scene.CreateEntityWithUUID(kNullUuid, "Nope"));

    scene.DestroyEntity(first);
    CHECK(scene.FindByUUID(first.GetUUID()));
    scene.FlushDestroyed();
    CHECK_FALSE(scene.FindByUUID(first.GetUUID()));
    CHECK(scene.FindByUUID(named.GetUUID()));
}

TEST_CASE("deferred destruction during iteration")
{
    Scene scene;
    const Entity a = scene.CreateEntity("A");
    const Entity b = scene.CreateEntity("B");
    const Entity c = scene.CreateEntity("C");
    int seen = 0;
    for (const auto entity : scene.View<TagComponent>())
    {
        scene.DestroyEntity(Entity{entity, &scene});
        ++seen;
    }
    CHECK(seen == 3);
    CHECK(scene.FindByUUID(a.GetUUID()));
    CHECK(scene.FindByUUID(b.GetUUID()));
    CHECK(scene.FindByUUID(c.GetUUID()));
    scene.FlushDestroyed();
    CHECK(scene.CreationOrder().empty());
    CHECK_FALSE(scene.FindByUUID(a.GetUUID()));
}

TEST_CASE("hierarchy reparent cycle and destroy")
{
    Scene scene;
    Entity root = scene.CreateEntity("Root");
    Entity child = scene.CreateEntity("Child");
    Entity grandchild = scene.CreateEntity("Grandchild");
    root.Get<TransformComponent>().position = {1.0f, 0.0f, 0.0f};
    child.Get<TransformComponent>().position = {0.0f, 2.0f, 0.0f};
    grandchild.Get<TransformComponent>().position = {0.0f, 0.0f, 3.0f};

    REQUIRE(scene.SetParent(child, root, false));
    REQUIRE(scene.SetParent(grandchild, child, false));
    CHECK_FALSE(scene.SetParent(root, grandchild, false));
    CHECK_FALSE(scene.SetParent(root, child, false));
    CHECK_FALSE(scene.SetParent(child, child, false));

    const TransformComponent world = scene.GetWorldTransform(grandchild);
    CHECK(world.position.x == doctest::Approx(1.0f));
    CHECK(world.position.y == doctest::Approx(2.0f));
    CHECK(world.position.z == doctest::Approx(3.0f));

    grandchild.Get<TransformComponent>().position = {4.0f, 0.0f, 0.0f};
    REQUIRE(scene.SetParent(grandchild, root, true));
    const TransformComponent kept = scene.GetWorldTransform(grandchild);
    CHECK(kept.position.x == doctest::Approx(5.0f));
    CHECK(kept.position.y == doctest::Approx(2.0f));
    CHECK(kept.position.z == doctest::Approx(0.0f));
    CHECK(grandchild.Get<RelationshipComponent>().parent == root.GetUUID());
    CHECK(child.Get<RelationshipComponent>().children.empty());

    const UUID child_id = child.GetUUID();
    const UUID grand_id = grandchild.GetUUID();
    scene.DestroyEntity(root);
    scene.FlushDestroyed();
    CHECK_FALSE(scene.FindByUUID(child_id));
    CHECK_FALSE(scene.FindByUUID(grand_id));
    CHECK(scene.CreationOrder().empty());
}

TEST_CASE("keep-world parent and unparent preserve world pose")
{
    Scene scene;
    Entity parent = scene.CreateEntity("Parent");
    Entity child = scene.CreateEntity("Child");
    parent.Get<TransformComponent>().position = {10.0f, 0.0f, 0.0f};
    parent.Get<TransformComponent>().rotation = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    child.Get<TransformComponent>().position = {1.0f, 2.0f, 0.0f};
    const TransformComponent before = scene.GetWorldTransform(child);

    REQUIRE(scene.SetParent(child, parent, true));
    const TransformComponent parented = scene.GetWorldTransform(child);
    CHECK(parented.position.x == doctest::Approx(before.position.x).epsilon(1.0e-4));
    CHECK(parented.position.y == doctest::Approx(before.position.y).epsilon(1.0e-4));
    CHECK(parented.position.z == doctest::Approx(before.position.z).epsilon(1.0e-4));
    CHECK(child.Get<RelationshipComponent>().parent == parent.GetUUID());

    REQUIRE(scene.SetParent(child, {}, true));
    const TransformComponent freed = scene.GetWorldTransform(child);
    CHECK(freed.position.x == doctest::Approx(before.position.x).epsilon(1.0e-4));
    CHECK(freed.position.y == doctest::Approx(before.position.y).epsilon(1.0e-4));
    CHECK(freed.position.z == doctest::Approx(before.position.z).epsilon(1.0e-4));
    CHECK(child.Get<RelationshipComponent>().parent == kNullUuid);
    CHECK(parent.Get<RelationshipComponent>().children.empty());
}

TEST_CASE("SetWorldTransform writes local through a rotated parent")
{
    Scene scene;
    Entity parent = scene.CreateEntity("Parent");
    Entity child = scene.CreateEntity("Child");
    parent.Get<TransformComponent>().position = {10.0f, 0.0f, 0.0f};
    parent.Get<TransformComponent>().rotation = quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f);
    REQUIRE(scene.SetParent(child, parent, false));

    TransformComponent world;
    world.position = {10.0f, 4.0f, -2.0f};
    scene.SetWorldTransform(child, world);
    const TransformComponent got = scene.GetWorldTransform(child);
    CHECK(got.position.x == doctest::Approx(10.0f).epsilon(1.0e-4));
    CHECK(got.position.y == doctest::Approx(4.0f).epsilon(1.0e-4));
    CHECK(got.position.z == doctest::Approx(-2.0f).epsilon(1.0e-4));
}

TEST_CASE("a stored parent cycle does not hang world lookup")
{
    Scene scene;
    Entity a = scene.CreateEntity("A");
    Entity b = scene.CreateEntity("B");
    a.Get<TransformComponent>().position = {1.0f, 0.0f, 0.0f};
    b.Get<TransformComponent>().position = {0.0f, 2.0f, 0.0f};
    a.Get<RelationshipComponent>().parent = b.GetUUID();
    b.Get<RelationshipComponent>().parent = a.GetUUID();
    const TransformComponent world = scene.GetWorldTransform(a);
    CHECK_FALSE(std::isnan(world.position.x));
    CHECK_FALSE(std::isnan(world.position.y));
}

TEST_CASE("scene copy preserves uuids and survives moves")
{
    Scene scene;
    Entity cube = scene.CreateEntityWithUUID(42, "Cube");
    cube.Get<TransformComponent>().position = {0.0f, 1.0f, 0.0f};
    cube.Add<PrimitiveBoxComponent>();
    cube.Add<SelectionOutlineTag>();
    Entity child = scene.CreateEntity("Child");
    REQUIRE(scene.SetParent(child, cube, false));

    Scene copy = scene;
    CHECK(copy == scene);
    CHECK(copy.FindByUUID(42).Get<TagComponent>().name == "Cube");
    CHECK(copy.FindByUUID(42).Has<PrimitiveBoxComponent>());
    CHECK(copy.FindByUUID(42).Has<SelectionOutlineTag>());
    CHECK(copy.FindByUUID(child.GetUUID()).Get<RelationshipComponent>().parent == 42);

    const std::uint64_t copy_library = copy.mesh_library();
    std::vector<Scene> moved;
    moved.push_back(std::move(copy));
    CHECK(moved.front().mesh_library() == copy_library);
    CHECK(copy.mesh_library() != copy_library);
    moved.push_back(Scene{});
    moved.push_back(Scene{});
    moved.erase(moved.begin() + 1);
    CHECK(moved.front().mesh_library() == copy_library);
    CHECK(moved.front().FindByUUID(42));
    CHECK(moved.front().FindByName("Cube").Get<TransformComponent>().position.y == doctest::Approx(1.0f));

    Scene donor = scene;
    const std::uint64_t donor_library = donor.mesh_library();
    Scene receiver;
    const std::uint64_t receiver_before = receiver.mesh_library();
    receiver = std::move(donor);
    CHECK(receiver.mesh_library() == donor_library);
    CHECK(donor.mesh_library() != donor_library);
    CHECK(donor.mesh_library() != receiver_before);

    Scene constructed(std::move(receiver));
    CHECK(constructed.mesh_library() == donor_library);
    CHECK(receiver.mesh_library() != donor_library);
    CHECK(receiver.mesh_library() != receiver_before);
}
