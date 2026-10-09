#include "persistence/project_file.hpp"
#include "persistence/sidecar.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace openphysx;
namespace fs = std::filesystem;

TEST_CASE("assets and caches live beside the project and a missing asset is a warning")
{
    const fs::path dir = fs::temp_directory_path() / "openphysx-sidecar";
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);
    const fs::path source = dir / "arm.obj";
    {
        std::ofstream output(source, std::ios::binary);
        output << "o arm\n";
    }
    const fs::path project_path = dir / "Robot.opx";
    const auto imported = ImportAsset(project_path, source, 0x42ull);
    REQUIRE(imported.has_value());
    CHECK(imported->path == "assets/arm.obj");
    CHECK(fs::exists(SidecarDir(project_path) / "assets" / "arm.obj"));

    const float samples[] = {1.0f, -2.5f, 4.0f};
    REQUIRE(WriteCache(project_path, "flow", samples));
    const auto loaded = ReadCache(project_path, "flow");
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 3);
    CHECK(loaded->at(1) == doctest::Approx(-2.5f));

    const auto missing = ReadCache(project_path, "absent");
    CHECK_FALSE(missing.has_value());
    CHECK(missing.error().kind == LoadError::Kind::NotFound);

    Project project;
    project.assets.push_back(*imported);
    project.assets.push_back(AssetRecord{0x43ull, "assets/gone.obj", "xxh64:0000000000000000"});
    project.created_utc = "2026-10-10T00:00:00Z";
    project.scene.CreateEntity("Base");
    REQUIRE(ProjectFile::Save(project_path, project));
    Project round;
    const auto report = ProjectFile::Load(project_path, round);
    REQUIRE(report.has_value());
    bool saw_missing = false;
    for (const LoadReport::Entry& entry : report->entries)
    {
        if (entry.message.find("gone.obj") != std::string::npos)
            saw_missing = true;
    }
    CHECK(saw_missing);
    CHECK(round.assets.size() == 2);

    fs::remove_all(dir, error);
    fs::remove_all(SidecarDir(project_path), error);
}
