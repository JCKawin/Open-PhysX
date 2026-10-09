#include "persistence/atomic_write.hpp"
#include "persistence/checksum.hpp"
#include "persistence/project_file.hpp"

#include <nlohmann/json.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace openphysx;
namespace fs = std::filesystem;

namespace {

fs::path test_dir(const char* name)
{
    const fs::path dir = fs::temp_directory_path() / name;
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);
    return dir;
}

std::string read_file(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

nlohmann::json normalized(const fs::path& path)
{
    nlohmann::json json = nlohmann::json::parse(read_file(path));
    json["modified_utc"] = "";
    return json;
}

Project sample_project()
{
    Project project;
    Entity cube = project.scene.CreateEntityWithUUID(0xA1ull, "Cube");
    cube.Get<TransformComponent>().position = {0.0f, 1.0f, 0.0f};
    cube.Add<PrimitiveBoxComponent>();
    project.gravity = {0.0f, -9.81f, 0.0f};
    project.timestep = 0.001f;
    project.selection = {0xA1ull};
    project.assets.push_back(AssetRecord{0xB2ull, "assets/arm.obj", "xxh64:0000000000000001"});
    project.camera.distance = 8.0f;
    project.created_utc = "2026-10-10T12:00:00Z";
    return project;
}

} // namespace

TEST_CASE("xxh64 matches published vectors")
{
    CHECK(Xxh64("") == 0xEF46DB3751D8E999ULL);
    CHECK(Xxh64("a") == 0xD24EC4F1A98C6E5BULL);
    CHECK(Xxh64("abc") == 0x44BC2CF5AD770999ULL);
    CHECK(Xxh64("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") == 0xF06103773E8585DFULL);
    CHECK(FormatChecksum(0xEF46DB3751D8E999ULL) == "xxh64:ef46db3751d8e999");
}

TEST_CASE("atomic write keeps the original when the rename is aborted")
{
    const fs::path dir = test_dir("openphysx-atomic");
    const fs::path file = dir / "Robot.opx";
    REQUIRE(AtomicWrite(file, "one"));
    CHECK_FALSE(fs::exists(file.wstring() + L".bak"));

    REQUIRE(AtomicWrite(file, "two"));
    CHECK(read_file(file) == "two");
    CHECK(read_file(file.wstring() + L".bak") == "one");

    SetAtomicFailPoint(AtomicFailPoint::AfterTempWrite);
    const auto failed = AtomicWrite(file, "three");
    SetAtomicFailPoint(AtomicFailPoint::None);
    CHECK_FALSE(failed.has_value());
    CHECK(read_file(file) == "two");
    CHECK(read_file(file.wstring() + L".bak") == "one");
    CHECK_FALSE(fs::exists(file.wstring() + L".tmp"));

    const fs::path unicode = dir / L"caf\u00e9.opx";
    REQUIRE(AtomicWrite(unicode, "ok"));
    CHECK(read_file(unicode) == "ok");
    std::error_code error;
    fs::remove_all(dir, error);
}

TEST_CASE("project save is checksummed, atomic, and stable apart from the timestamp")
{
    const fs::path dir = test_dir("openphysx-project");
    const fs::path file = dir / "Robot.opx";
    const Project project = sample_project();
    REQUIRE(ProjectFile::Save(file, project));
    REQUIRE(ProjectFile::Save(file, project));
    CHECK(normalized(file) == normalized(file.wstring() + L".bak"));

    Project loaded;
    const auto report = ProjectFile::Load(file, loaded);
    REQUIRE(report.has_value());
    CHECK(report->entries.empty());
    CHECK(loaded.scene.FindByUUID(0xA1ull).Get<TagComponent>().name == "Cube");
    CHECK(loaded.scene.FindByUUID(0xA1ull).Get<TransformComponent>().position.y == doctest::Approx(1.0f));
    CHECK(loaded.gravity.y == doctest::Approx(-9.81f));
    CHECK(loaded.selection.size() == 1);
    CHECK(loaded.selection.front() == 0xA1ull);
    CHECK(loaded.assets.size() == 1);
    CHECK(loaded.assets.front().path == "assets/arm.obj");
    CHECK(loaded.camera.distance == doctest::Approx(8.0f));
    CHECK(loaded.created_utc == "2026-10-10T12:00:00Z");

    std::string text = read_file(file);
    const auto mark = text.find("xxh64:");
    REQUIRE(mark != std::string::npos);
    text[mark + 6] = text[mark + 6] == '0' ? '1' : '0';
    {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << text;
    }

    Project untouched = sample_project();
    const UUID keep = untouched.scene.CreateEntity("Keep").GetUUID();
    const auto corrupt = ProjectFile::Load(file, untouched);
    CHECK_FALSE(corrupt.has_value());
    CHECK(corrupt.error().kind == LoadError::Kind::Corrupt);
    CHECK(corrupt.error().message.find("backup") != std::string::npos);
    CHECK(untouched.scene.FindByUUID(keep));

    Project restored;
    const auto from_backup = ProjectFile::Load(file.wstring() + L".bak", restored);
    REQUIRE(from_backup.has_value());
    CHECK(restored.scene.FindByUUID(0xA1ull));

    std::ofstream empty(dir / "empty.opx", std::ios::binary | std::ios::trunc);
    empty.close();
    Project ignored;
    const auto empty_load = ProjectFile::Load(dir / "empty.opx", ignored);
    CHECK_FALSE(empty_load.has_value());
    CHECK(empty_load.error().kind == LoadError::Kind::Corrupt);

    std::ofstream wrong(dir / "wrong.opx", std::ios::binary | std::ios::trunc);
    wrong << "{\"magic\":\"NOPE\",\"format_version\":1}\n";
    wrong.close();
    const auto wrong_load = ProjectFile::Load(dir / "wrong.opx", ignored);
    CHECK(wrong_load.error().kind == LoadError::Kind::Schema);

    std::ofstream newer(dir / "newer.opx", std::ios::binary | std::ios::trunc);
    newer << "{\"magic\":\"OPX\",\"format_version\":99}\n";
    newer.close();
    const auto newer_load = ProjectFile::Load(dir / "newer.opx", ignored);
    CHECK(newer_load.error().kind == LoadError::Kind::TooNew);

    const auto missing = ProjectFile::Load(dir / "missing.opx", ignored);
    CHECK(missing.error().kind == LoadError::Kind::NotFound);

    std::error_code error;
    fs::remove_all(dir, error);
}
