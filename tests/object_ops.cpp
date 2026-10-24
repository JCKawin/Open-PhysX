#include "core/Math.h"
#include "core/Mesh.h"
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

TEST_CASE("a new object has every slot empty and the scene is at version 2")
{
    Scene scene;
    CHECK(scene.Version() == 2);
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
    CHECK(body.size.z == doctest::Approx(2.0f));
    CHECK(body.scale.x == doctest::Approx(2.0f));
    CHECK(body.scale.y == doctest::Approx(0.5f));
    CHECK(body.scale.z == doctest::Approx(1.0f));
    CHECK(cube.Get<PrimitiveBoxComponent>().size.x == doctest::Approx(2.0f));
}

TEST_CASE("mesh draw axes carry object scale and not the box size")
{
    Vec3 x_axis;
    Vec3 y_axis;
    Vec3 z_axis;
    scaled_axes({}, {2.0f, 0.5f, 1.0f}, x_axis, y_axis, z_axis);
    CHECK(x_axis.x == doctest::Approx(2.0f));
    CHECK(x_axis.y == doctest::Approx(0.0f));
    CHECK(x_axis.z == doctest::Approx(0.0f));
    CHECK(y_axis.y == doctest::Approx(0.5f));
    CHECK(z_axis.z == doctest::Approx(1.0f));
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

TEST_CASE("mesh ids survive removal of another mesh and are not reused")
{
    Scene scene;
    Mesh small = make_cube({1.0f, 1.0f, 1.0f});
    Mesh large = make_cube({2.0f, 2.0f, 2.0f});
    const float large_x = large.positions.front().x;
    const uint32_t first = scene.add_mesh(std::move(small));
    const uint32_t second = scene.add_mesh(std::move(large));
    CHECK(first != 0);
    CHECK(second != first);

    REQUIRE(scene.remove_mesh(first));
    CHECK_FALSE(scene.remove_mesh(first));
    CHECK(scene.find_mesh(first) == nullptr);
    const Mesh* kept = scene.find_mesh(second);
    REQUIRE(kept != nullptr);
    CHECK(kept->positions.front().x == doctest::Approx(large_x));

    const uint32_t third = scene.add_mesh(make_cube({3.0f, 3.0f, 3.0f}));
    CHECK(third != first);
    CHECK(third != second);
    CHECK(scene.find_mesh(0) == nullptr);

    const std::uint64_t library = scene.mesh_library();
    Scene copy = scene;
    CHECK(copy.mesh_library() == library);
    CHECK(copy.find_mesh(second) != nullptr);
    REQUIRE(copy.remove_mesh(second));
    CHECK(scene.find_mesh(second) != nullptr);
    CHECK(copy.find_mesh(second) == nullptr);
    const uint32_t on_copy = copy.add_mesh(make_cube({4.0f, 4.0f, 4.0f}));
    CHECK(on_copy != first);
    CHECK(on_copy != second);
    CHECK(on_copy != third);

    Scene fresh;
    CHECK(fresh.mesh_library() != library);
}

TEST_CASE("undo restores a removed mesh id")
{
    Simulation simulation;
    CommandStack stack;
    Scene& scene = simulation.editor_scene();
    const uint32_t removed = scene.add_mesh(make_cube({4.0f, 4.0f, 4.0f}));
    REQUIRE(scene.remove_mesh(removed));

    Mesh cube = make_cube({1.0f, 1.0f, 1.0f});
    const float vertex_x = cube.positions.front().x;
    const uint32_t mesh_id = scene.add_mesh(std::move(cube));
    const std::uint64_t revision = scene.mesh_revision();
    const EditSnapshot before = capture(simulation);

    REQUIRE(scene.remove_mesh(mesh_id));
    stack.Commit(before, capture(simulation), false);
    CHECK(simulation.editor_scene().find_mesh(mesh_id) == nullptr);

    stack.Undo(simulation);
    const Mesh* restored = simulation.editor_scene().find_mesh(mesh_id);
    REQUIRE(restored != nullptr);
    CHECK(restored->positions.front().x == doctest::Approx(vertex_x));
    CHECK(simulation.editor_scene().mesh_revision() == revision);
    const uint32_t fresh = simulation.editor_scene().add_mesh(make_cube({5.0f, 5.0f, 5.0f}));
    CHECK(fresh != mesh_id);
    CHECK(fresh != removed);

    stack.Redo(simulation);
    CHECK(simulation.editor_scene().find_mesh(mesh_id) == nullptr);
}

TEST_CASE("a deleted joint target and constraint target resolve as missing")
{
    Scene scene;
    Entity body_a = make_box(scene, "A", {});
    Entity body_b = make_box(scene, "B", {});
    Entity owner = make_box(scene, "Owner", {});
    const UUID id_a = body_a.GetUUID();
    const UUID id_b = body_b.GetUUID();

    Entity joint = scene.CreateEntity("Joint");
    JointComponent joint_value;
    joint_value.bodyA = id_a;
    joint_value.bodyB = id_b;
    joint.Add<JointComponent>(joint_value);
    const UUID joint_id = joint.GetUUID();

    ConstraintEntry row;
    row.id = 0xC011ull;
    row.target = id_a;
    owner.Get<ConstraintStackComponent>().entries.push_back(row);
    ConstraintEntry unassigned;
    unassigned.id = kNullUuid;
    unassigned.target = id_b;
    owner.Get<ConstraintStackComponent>().entries.push_back(unassigned);

    CHECK(scene.FindByUUID(id_a));
    CHECK_FALSE(scene.FindByUUID(kNullUuid));
    CHECK_FALSE(scene.FindConstraintOwner(kNullUuid));
    CHECK(scene.FindConstraintOwner(row.id).GetUUID() == owner.GetUUID());

    DeleteObjects(scene, {id_a});

    CHECK_FALSE(scene.FindByUUID(id_a));
    CHECK(id_a != kNullUuid);
    CHECK(scene.FindByUUID(id_b));
    const Entity joint_after = scene.FindByUUID(joint_id);
    REQUIRE(joint_after);
    CHECK(joint_after.Get<JointComponent>().bodyA == id_a);
    CHECK_FALSE(scene.FindByUUID(joint_after.Get<JointComponent>().bodyA));
    CHECK(scene.FindByUUID(joint_after.Get<JointComponent>().bodyB));

    const Entity owner_after = scene.FindConstraintOwner(row.id);
    REQUIRE(owner_after);
    REQUIRE(owner_after.Get<ConstraintStackComponent>().entries.size() == 2);
    CHECK(owner_after.Get<ConstraintStackComponent>().entries.front().target == id_a);
    CHECK_FALSE(scene.FindByUUID(owner_after.Get<ConstraintStackComponent>().entries.front().target));

    DeleteObjects(scene, {owner_after.GetUUID()});
    CHECK_FALSE(scene.FindConstraintOwner(row.id));
}

TEST_CASE("undo of deleting a parent restores the parent link")
{
    Simulation simulation;
    CommandStack stack;
    Scene& scene = simulation.editor_scene();
    Entity parent = make_box(scene, "Parent", {1.0f, 0.0f, 0.0f});
    Entity child = make_box(scene, "Child", {0.0f, 2.0f, 0.0f});
    REQUIRE(scene.SetParent(child, parent, false));
    const UUID parent_id = parent.GetUUID();
    const UUID child_id = child.GetUUID();
    const Vec3 local_before = child.Get<TransformComponent>().position;
    const std::vector<UUID> children_before = parent.Get<RelationshipComponent>().children;
    CHECK(local_before.x == doctest::Approx(0.0f));
    CHECK(local_before.y == doctest::Approx(2.0f));
    CHECK(local_before.z == doctest::Approx(0.0f));
    CHECK(contains(children_before, child_id));
    const TransformComponent world_before = scene.GetWorldTransform(child);
    const EditSnapshot before = capture(simulation);

    DeleteObjects(scene, {parent_id});
    simulation.set_active(child_id);
    stack.Commit(before, capture(simulation), false);
    CHECK_FALSE(simulation.editor_scene().FindByUUID(parent_id));
    CHECK(simulation.editor_scene().FindByUUID(child_id).Get<RelationshipComponent>().parent == kNullUuid);

    stack.Undo(simulation);
    const Entity restored_child = simulation.editor_scene().FindByUUID(child_id);
    const Entity restored_parent = simulation.editor_scene().FindByUUID(parent_id);
    REQUIRE(restored_parent);
    REQUIRE(restored_child);
    CHECK(restored_child.Get<RelationshipComponent>().parent == parent_id);
    CHECK(restored_child.Get<TransformComponent>().position.x == doctest::Approx(0.0f));
    CHECK(restored_child.Get<TransformComponent>().position.y == doctest::Approx(2.0f));
    CHECK(restored_child.Get<TransformComponent>().position.z == doctest::Approx(0.0f));
    CHECK(contains(restored_parent.Get<RelationshipComponent>().children, child_id));

    stack.Redo(simulation);
    CHECK_FALSE(simulation.editor_scene().FindByUUID(parent_id));
    const Entity baked = simulation.editor_scene().FindByUUID(child_id);
    REQUIRE(baked);
    CHECK(baked.Get<RelationshipComponent>().parent == kNullUuid);
    CHECK(baked.Get<TransformComponent>().position.x == doctest::Approx(world_before.position.x));
    CHECK(baked.Get<TransformComponent>().position.y == doctest::Approx(world_before.position.y));
    CHECK(baked.Get<TransformComponent>().position.z == doctest::Approx(world_before.position.z));
}

TEST_CASE("a destroyed entity handle stays stale after the id is reused by the registry")
{
    Scene scene;
    Entity stale = scene.CreateEntity("Gone");
    const UUID stale_id = stale.GetUUID();
    const entt::entity raw = stale.Raw();
    scene.DestroyEntity(stale);
    CHECK(stale);
    scene.FlushDestroyed();
    CHECK_FALSE(stale);
    CHECK(stale.GetUUID() == kNullUuid);
    CHECK_FALSE(scene.FindByUUID(stale_id));
    CHECK(stale_id != kNullUuid);

    Entity fresh = scene.CreateEntity("Next");
    CHECK(fresh);
    CHECK(fresh.GetUUID() != stale_id);
    CHECK(entt::to_entity(fresh.Raw()) == entt::to_entity(raw));
    CHECK(fresh.Raw() != raw);
    CHECK_FALSE(stale);
    CHECK_FALSE(scene.Registry().valid(raw));
}

TEST_CASE("duplicating a constraint row keeps the target and issues a new row id")
{
    Scene scene;
    Entity owner = make_box(scene, "Owner", {});
    Entity target = make_box(scene, "Target", {});
    ConstraintEntry row;
    row.id = 0xA11ull;
    row.target = target.GetUUID();
    owner.Get<ConstraintStackComponent>().entries.push_back(row);

    const std::vector<UUID> copies = DuplicateObjects(scene, {owner.GetUUID()});
    REQUIRE(copies.size() == 1);
    const Entity copy = scene.FindByUUID(copies.front());
    REQUIRE(copy.Get<ConstraintStackComponent>().entries.size() == 1);
    const ConstraintEntry& copied = copy.Get<ConstraintStackComponent>().entries.front();
    CHECK(copied.id != row.id);
    CHECK(copied.id != kNullUuid);
    CHECK(copied.target == target.GetUUID());
    CHECK(scene.FindConstraintOwner(row.id).GetUUID() == owner.GetUUID());
    CHECK(scene.FindConstraintOwner(copied.id).GetUUID() == copy.GetUUID());
}

TEST_CASE("a missing parent does not crash world transform lookup")
{
    Scene scene;
    Entity child = make_box(scene, "Child", {0.0f, 2.0f, 0.0f});
    const UUID missing = 0xBEEFull;
    child.Get<RelationshipComponent>().parent = missing;
    CHECK(missing != kNullUuid);
    CHECK_FALSE(scene.FindByUUID(missing));
    const TransformComponent world = scene.GetWorldTransform(child);
    CHECK(world.position.y == doctest::Approx(2.0f));
}
