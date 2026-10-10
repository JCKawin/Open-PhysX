#include "ecs/components/physics.hpp"
#include "ecs/components/render.hpp"
#include "ecs/components/slots.hpp"
#include "ecs/object_ops.hpp"
#include "ecs/pose.hpp"
#include "editor/command.hpp"
#include "logic/Simulation.h"

#include <doctest/doctest.h>

#include <algorithm>

using namespace openphysx;

namespace {

EditSnapshot capture(const Simulation& simulation)
{
    EditSnapshot shot;
    shot.state = simulation.state();
    shot.scene = simulation.editor_scene();
    shot.active = simulation.active_id();
    return shot;
}

bool contains(const std::vector<UUID>& ids, UUID id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

Entity make_box(Scene& scene, const char* name, Vec3 position)
{
    Entity entity = scene.CreateEntity(name);
    entity.Get<TransformComponent>().position = position;
    entity.Add<PrimitiveBoxComponent>();
    return entity;
}

} // namespace

TEST_CASE("a new object has every slot empty and the scene is at version 1")
{
    Scene scene;
    CHECK(scene.Version() == 1);
    const Entity cube = scene.FindByUUID(AddBoxObject(scene));
    REQUIRE(cube);
    CHECK(cube.Get<MeshRendererComponent>().meshAsset == kNullUuid);
    CHECK_FALSE(cube.Has<RigidBodyComponent>());
    CHECK(cube.Get<ConstraintStackComponent>().entries.empty());
    CHECK(cube.Get<ModifierStackComponent>().entries.empty());
    CHECK(cube.Get<FluidRoleComponent>().role == FluidRole::None);
    CHECK(EntitySelected(cube));
}

TEST_CASE("object names stay unique and follow the .NNN suffix rule")
{
    Scene scene;
    const Entity first = scene.CreateEntity("Cube");
    const Entity second = scene.CreateEntity("Cube");
    const Entity third = scene.CreateEntity("Cube");
    CHECK(first.Get<TagComponent>().name == "Cube");
    CHECK(second.Get<TagComponent>().name == "Cube.001");
    CHECK(third.Get<TagComponent>().name == "Cube.002");

    // A rename to a taken name takes the next free suffix. An object does not clash with itself.
    CHECK(scene.SetName(third, "Cube") == "Cube.002");
    CHECK(scene.SetName(first, "Cube.001") == "Cube.003");
    CHECK(scene.CreateEntity("Cube.002").Get<TagComponent>().name == "Cube.004");
}

TEST_CASE("a deleted object's id is not handed out again")
{
    Scene scene;
    const UUID old_id = scene.CreateEntity("Gone").GetUUID();
    DeleteObjects(scene, {old_id});
    CHECK_FALSE(scene.FindByUUID(old_id));
    const UUID fresh = scene.CreateEntity("Gone").GetUUID();
    CHECK(fresh != kNullUuid);
    CHECK(fresh != old_id);
}

TEST_CASE("duplicating copies values under a new id and name and keeps the parent")
{
    Scene scene;
    Entity base = make_box(scene, "Base", {1.0f, 2.0f, 3.0f});
    base.Get<TransformComponent>().scale = {2.0f, 1.0f, 1.0f};
    base.Get<MeshRendererComponent>().castShadow = false;
    Entity child = make_box(scene, "Child", {0.0f, 1.0f, 0.0f});
    REQUIRE(scene.SetParent(child, base, false));

    const std::vector<UUID> copies = DuplicateObjects(scene, {base.GetUUID()});
    REQUIRE(copies.size() == 1);
    const Entity copy = scene.FindByUUID(copies.front());
    REQUIRE(copy);
    CHECK(copy.GetUUID() != base.GetUUID());
    CHECK(copy.Get<TagComponent>().name == "Base.001");
    CHECK(copy.Get<TransformComponent>().position.x == 1.0f);
    CHECK(copy.Get<TransformComponent>().scale.x == 2.0f);
    CHECK_FALSE(copy.Get<MeshRendererComponent>().castShadow);
    CHECK(copy.Get<RelationshipComponent>().parent == kNullUuid);
    CHECK(EntitySelected(copy));
    CHECK_FALSE(EntitySelected(base));
    CHECK(child.Get<RelationshipComponent>().parent == base.GetUUID());
    CHECK(base.Get<RelationshipComponent>().children.size() == 1);
}

TEST_CASE("a duplicated child stays under its parent")
{
    Scene scene;
    Entity base = make_box(scene, "Base", {});
    Entity child = make_box(scene, "Child", {0.0f, 1.0f, 0.0f});
    REQUIRE(scene.SetParent(child, base, false));

    const std::vector<UUID> copies = DuplicateObjects(scene, {child.GetUUID()});
    const Entity copy = scene.FindByUUID(copies.front());
    CHECK(copy.Get<RelationshipComponent>().parent == base.GetUUID());
    CHECK(contains(base.Get<RelationshipComponent>().children, copy.GetUUID()));
}

TEST_CASE("duplicating a parent with its child copies the link between the copies")
{
    Scene scene;
    Entity base = make_box(scene, "Base", {});
    Entity child = make_box(scene, "Child", {0.0f, 1.0f, 0.0f});
    REQUIRE(scene.SetParent(child, base, false));

    const std::vector<UUID> copies = DuplicateObjects(scene, {base.GetUUID(), child.GetUUID()});
    REQUIRE(copies.size() == 2);
    const Entity base_copy = scene.FindByUUID(copies[0]);
    const Entity child_copy = scene.FindByUUID(copies[1]);
    CHECK(base_copy.Get<RelationshipComponent>().parent == kNullUuid);
    CHECK(child_copy.Get<RelationshipComponent>().parent == base_copy.GetUUID());
    CHECK(child.Get<RelationshipComponent>().parent == base.GetUUID());
}

TEST_CASE("deleting a parent keeps its children at their world position")
{
    Scene scene;
    Entity parent = make_box(scene, "Parent", {1.0f, 0.0f, 0.0f});
    Entity child = make_box(scene, "Child", {0.0f, 2.0f, 0.0f});
    REQUIRE(scene.SetParent(child, parent, false));
    const UUID child_id = child.GetUUID();
    const TransformComponent before = scene.GetWorldTransform(child);

    DeleteObjects(scene, {parent.GetUUID()});

    const Entity survivor = scene.FindByUUID(child_id);
    REQUIRE(survivor);
    CHECK_FALSE(scene.FindByName("Parent"));
    CHECK(survivor.Get<RelationshipComponent>().parent == kNullUuid);
    CHECK(survivor.Get<TransformComponent>().position.x == doctest::Approx(before.position.x));
    CHECK(survivor.Get<TransformComponent>().position.y == doctest::Approx(before.position.y));
    CHECK(survivor.Get<TransformComponent>().position.z == doctest::Approx(before.position.z));
}

TEST_CASE("a hidden parent hides its children in the world but not their own flag")
{
    Scene scene;
    Entity parent = make_box(scene, "Parent", {});
    Entity child = make_box(scene, "Child", {0.0f, 1.0f, 0.0f});
    REQUIRE(scene.SetParent(child, parent, false));
    parent.Get<EditorStateComponent>().visible = false;

    CHECK(child.Get<EditorStateComponent>().visible);
    CHECK_FALSE(IsVisibleInWorld(scene, child));
    CHECK(DrawableObjects(scene).empty());

    parent.Get<EditorStateComponent>().visible = true;
    CHECK(DrawableObjects(scene).size() == 2);
}

TEST_CASE("select all takes every visible object and no hidden one")
{
    Scene scene;
    Entity a = make_box(scene, "A", {});
    Entity b = make_box(scene, "B", {});
    Entity c = make_box(scene, "C", {});
    c.Get<EditorStateComponent>().visible = false;

    SelectAllVisible(scene);
    CHECK(EntitySelected(a));
    CHECK(EntitySelected(b));
    CHECK_FALSE(EntitySelected(c));
    CHECK(SelectedObjects(scene) == std::vector<UUID>{a.GetUUID(), b.GetUUID()});

    DeselectAll(scene);
    CHECK(SelectedObjects(scene).empty());
}

TEST_CASE("a selected child follows its selected parent and is not a transform root")
{
    Scene scene;
    Entity parent = make_box(scene, "Parent", {});
    Entity child = make_box(scene, "Child", {0.0f, 1.0f, 0.0f});
    Entity other = make_box(scene, "Other", {5.0f, 0.0f, 0.0f});
    REQUIRE(scene.SetParent(child, parent, false));
    SetEntitySelected(parent, true);
    SetEntitySelected(child, true);
    SetEntitySelected(other, true);

    const std::vector<UUID> roots = TransformRoots(scene);
    CHECK(roots.size() == 2);
    CHECK(contains(roots, parent.GetUUID()));
    CHECK(contains(roots, other.GetUUID()));
    CHECK_FALSE(contains(roots, child.GetUUID()));
}

TEST_CASE("the active object is always a member of the selection")
{
    Scene scene;
    Entity a = make_box(scene, "A", {});
    Entity b = make_box(scene, "B", {});
    SetEntitySelected(a, true);
    SetEntitySelected(b, true);
    CHECK(ResolveActive(scene, a.GetUUID()) == a.GetUUID());

    SetEntitySelected(a, false);
    CHECK(ResolveActive(scene, a.GetUUID()) == b.GetUUID());

    SetEntitySelected(b, false);
    CHECK(ResolveActive(scene, a.GetUUID()) == kNullUuid);
}

TEST_CASE("show brings back every hidden object and selects it")
{
    Scene scene;
    Entity a = make_box(scene, "A", {});
    Entity b = make_box(scene, "B", {});
    a.Get<EditorStateComponent>().visible = false;
    b.Get<EditorStateComponent>().visible = false;
    SetEntitySelected(b, false);

    RevealHidden(scene);
    CHECK(a.Get<EditorStateComponent>().visible);
    CHECK(b.Get<EditorStateComponent>().visible);
    CHECK(EntitySelected(a));
    CHECK(EntitySelected(b));
}

TEST_CASE("a second object pushed into the scene is drawn with the first")
{
    Simulation simulation;
    CHECK(DrawableObjects(simulation.scene()).size() == 1);

    const UUID second = AddBoxObject(simulation.editor_scene());
    const std::vector<UUID> drawn = DrawableObjects(simulation.scene());
    CHECK(drawn.size() == 2);
    CHECK(contains(drawn, second));
    CHECK(simulation.visual_body_of(second).position.y == doctest::Approx(1.0f));
}

TEST_CASE("an empty scene has nothing to draw or transform")
{
    Simulation simulation;
    simulation.editor_scene() = Scene{};
    simulation.set_active(kNullUuid);
    CHECK(DrawableObjects(simulation.scene()).empty());
    CHECK(TransformRoots(simulation.scene()).empty());
    CHECK_FALSE(simulation.active_entity());
}

TEST_CASE("scale multiplies the box in the draw pose and stays on the transform")
{
    Scene scene;
    Entity cube = make_box(scene, "Cube", {});
    cube.Get<TransformComponent>().scale = {2.0f, 0.5f, 1.0f};

    const RigidBody body = WorldBody(scene, cube);
    CHECK(body.size.x == doctest::Approx(4.0f));
    CHECK(body.size.y == doctest::Approx(1.0f));
    CHECK(cube.Get<PrimitiveBoxComponent>().size.x == doctest::Approx(2.0f));
}

TEST_CASE("undo brings back a deleted object with the same id and selection")
{
    Simulation simulation;
    CommandStack stack;
    const UUID cube = simulation.active_id();
    const EditSnapshot before = capture(simulation);

    DeleteObjects(simulation.editor_scene(), {cube});
    simulation.set_active(kNullUuid);
    stack.Commit(before, capture(simulation), false);
    CHECK_FALSE(simulation.editor_scene().FindByUUID(cube));

    stack.Undo(simulation);
    REQUIRE(simulation.editor_scene().FindByUUID(cube));
    CHECK(simulation.active_id() == cube);
    CHECK(EntitySelected(simulation.editor_scene().FindByUUID(cube)));

    stack.Redo(simulation);
    CHECK_FALSE(simulation.editor_scene().FindByUUID(cube));
}

TEST_CASE("undo of a duplicate removes the copy and keeps the source id")
{
    Simulation simulation;
    CommandStack stack;
    const UUID source = simulation.active_id();
    const EditSnapshot before = capture(simulation);

    const std::vector<UUID> copies = DuplicateObjects(simulation.editor_scene(), {source});
    stack.Commit(before, capture(simulation), false);
    REQUIRE(copies.size() == 1);
    CHECK(simulation.editor_scene().CreationOrder().size() == 2);

    stack.Undo(simulation);
    CHECK(simulation.editor_scene().CreationOrder().size() == 1);
    CHECK(simulation.active_id() == source);

    stack.Redo(simulation);
    CHECK(simulation.editor_scene().FindByUUID(copies.front()));
}
