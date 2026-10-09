#include "ecs/pose.hpp"
#include "editor/command.hpp"
#include "logic/Simulation.h"

#include <doctest/doctest.h>

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

float active_x(const Simulation& simulation)
{
    const Entity entity = simulation.editor_scene().FindByUUID(simulation.active_id());
    REQUIRE(entity);
    return entity.Get<TransformComponent>().position.x;
}

} // namespace

TEST_CASE("command revision dirties on edit and cleans on undo back to the save")
{
    Simulation simulation;
    CommandStack stack;
    const EditSnapshot original = capture(simulation);

    simulation.editor_scene().FindByUUID(simulation.active_id()).Get<TransformComponent>().position.x = 3.0f;
    stack.Commit(original, capture(simulation), false);
    CHECK(stack.IsDirty());
    CHECK(stack.revision() == 1);

    stack.MarkSaved();
    CHECK_FALSE(stack.IsDirty());

    stack.Undo(simulation);
    CHECK(active_x(simulation) == doctest::Approx(0.0f));
    CHECK(stack.IsDirty());

    stack.Redo(simulation);
    CHECK(active_x(simulation) == doctest::Approx(3.0f));
    CHECK_FALSE(stack.IsDirty());

    const EditSnapshot saved = capture(simulation);
    simulation.editor_scene().FindByUUID(simulation.active_id()).Get<TransformComponent>().position.x = 8.0f;
    stack.Commit(saved, capture(simulation), false);
    CHECK(stack.IsDirty());
    stack.Undo(simulation);
    CHECK_FALSE(stack.IsDirty());
    CHECK(active_x(simulation) == doctest::Approx(3.0f));
}

TEST_CASE("selection and playback do not move the command revision")
{
    Simulation simulation;
    CommandStack stack;
    const std::uint64_t revision = stack.revision();

    Entity entity = simulation.scene().FindByUUID(simulation.active_id());
    SetEntitySelected(entity, false);
    simulation.play();
    simulation.scene().FindByUUID(simulation.active_id()).Get<TransformComponent>().position.x = 9.0f;
    simulation.step(0.1f);
    simulation.stop();

    CHECK(stack.revision() == revision);
    CHECK_FALSE(stack.IsDirty());
    CHECK(active_x(simulation) == doctest::Approx(0.0f));
}

TEST_CASE("clearing the command stack marks the document clean")
{
    Simulation simulation;
    CommandStack stack;
    const EditSnapshot before = capture(simulation);
    simulation.state().show_grid = false;
    stack.Commit(before, capture(simulation), false);
    CHECK(stack.IsDirty());
    stack.Clear();
    CHECK_FALSE(stack.IsDirty());
    CHECK_FALSE(stack.CanUndo());
    CHECK(stack.revision() == 0);
}
