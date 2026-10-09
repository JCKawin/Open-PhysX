#include "ecs/scene.hpp"

#include <doctest/doctest.h>

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

    std::vector<Scene> moved;
    moved.push_back(std::move(copy));
    moved.push_back(Scene{});
    moved.push_back(Scene{});
    moved.erase(moved.begin() + 1);
    CHECK(moved.front().FindByUUID(42));
    CHECK(moved.front().FindByName("Cube").Get<TransformComponent>().position.y == doctest::Approx(1.0f));
}
