#include "persistence/migrations.hpp"
#include "persistence/project_file.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace openphysx;
namespace fs = std::filesystem;

TEST_CASE("a v1 golden project loads and a migrated original is archived")
{
    const fs::path golden = fs::path("D:/projects/Open-PhysX/tests/data/v1/cube.opx");
    if (!fs::exists(golden))
    {
        Project project;
        Entity cube = project.scene.CreateEntityWithUUID(0xC0BEull, "Cube");
        cube.Get<TransformComponent>().position = {0.0f, 1.0f, 0.0f};
        cube.Add<PrimitiveBoxComponent>();
        project.created_utc = "2026-10-10T00:00:00Z";
        std::error_code error;
        fs::create_directories(golden.parent_path(), error);
        REQUIRE(ProjectFile::Save(golden, project));
    }

    Project loaded;
    const auto report = ProjectFile::Load(golden, loaded);
    REQUIRE(report.has_value());
    CHECK(loaded.source_version == 1);
    CHECK(loaded.format_version == Project::kFormatVersion);
    const Entity cube = loaded.scene.FindByName("Cube");
    REQUIRE(cube);
    CHECK(cube.GetUUID() == 0xC0BEull);
    CHECK(cube.Get<TransformComponent>().position.y == doctest::Approx(1.0f));
    CHECK(cube.Has<PrimitiveBoxComponent>());

    int source = 0;
    auto newer = MigrateProject(nlohmann::json{{"magic", "OPX"}, {"format_version", 99}}, source);
    CHECK_FALSE(newer.has_value());
    CHECK(newer.error().kind == LoadError::Kind::TooNew);

    const fs::path dir = fs::temp_directory_path() / "openphysx-migrate";
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);
    const fs::path original = dir / "Robot.opx";
    {
        std::ofstream output(original, std::ios::binary);
        output << "original-v1";
    }
    REQUIRE(ArchiveMigratedOriginal(original, 1));
    std::ifstream archived(dir / "Robot.v1.bak", std::ios::binary);
    std::string text;
    archived >> text;
    CHECK(text == "original-v1");
    fs::remove_all(dir, error);
}
