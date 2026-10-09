#include "logic/Simulation.h"
#include "persistence/project_manager.hpp"

#include <doctest/doctest.h>

#include <filesystem>

using namespace openphysx;
namespace fs = std::filesystem;

namespace {

EditSnapshot shot(const Simulation& simulation)
{
    EditSnapshot value;
    value.state = simulation.state();
    value.scene = simulation.editor_scene();
    value.active = simulation.active_id();
    return value;
}

fs::path test_dir()
{
    const fs::path dir = fs::temp_directory_path() / "openphysx-manager";
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);
    return dir;
}

} // namespace

TEST_CASE("save and open track the dirty revision and leave a failed open unchanged")
{
    const fs::path dir = test_dir();
    Simulation simulation;
    CommandStack commands;
    ProjectManager manager;
    const View3D view{};

    manager.New(simulation, commands);
    CHECK_FALSE(manager.has_path());
    CHECK(manager.DisplayName() == "Untitled");
    CHECK(manager.WindowTitle(commands) == "OpenPhysX - Untitled");
    CHECK_FALSE(commands.IsDirty());

    const EditSnapshot before = shot(simulation);
    const UUID id = simulation.active_id();
    simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position.x = 4.0f;
    commands.Commit(before, shot(simulation), false);
    CHECK(commands.IsDirty());
    CHECK(manager.WindowTitle(commands) == "OpenPhysX - Untitled*");

    const fs::path file = dir / "Arm.opx";
    REQUIRE(manager.SaveAs(file, simulation, commands, view, "layout"));
    CHECK_FALSE(commands.IsDirty());
    CHECK(manager.WindowTitle(commands) == "OpenPhysX - Arm.opx");
    CHECK(manager.path() == file);

    commands.Undo(simulation);
    CHECK(commands.IsDirty());
    CHECK(simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position.x == doctest::Approx(0.0f));
    commands.Redo(simulation);
    CHECK_FALSE(commands.IsDirty());
    CHECK(simulation.editor_scene().FindByUUID(id).Get<TransformComponent>().position.x == doctest::Approx(4.0f));

    simulation.play();
    simulation.scene().FindByUUID(id).Get<TransformComponent>().position.x = 9.0f;
    REQUIRE(manager.Save(simulation, commands, view, "layout"));
    simulation.stop();

    Simulation other;
    CommandStack other_commands;
    ProjectManager reader;
    const auto opened = reader.Open(file, other, other_commands);
    REQUIRE(opened.has_value());
    CHECK(other.editor_scene().FindByUUID(id).Get<TransformComponent>().position.x == doctest::Approx(4.0f));
    CHECK_FALSE(other_commands.IsDirty());
    CHECK(reader.layout_ini() == "layout");

    const UUID kept = other.active_id();
    const auto missing = reader.Open(dir / "missing.opx", other, other_commands);
    CHECK_FALSE(missing.has_value());
    CHECK(missing.error().kind == LoadError::Kind::NotFound);
    CHECK(reader.path() == file);
    CHECK(other.active_id() == kept);

    other.editor_scene().FindByUUID(id).Get<TransformComponent>().position.x = 1.0f;
    const auto reverted = reader.Revert(other, other_commands);
    REQUIRE(reverted.has_value());
    CHECK(other.editor_scene().FindByUUID(id).Get<TransformComponent>().position.x == doctest::Approx(4.0f));

    std::error_code error;
    fs::remove_all(dir, error);
}
