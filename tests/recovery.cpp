#include "logic/Simulation.h"
#include "persistence/project_file.hpp"
#include "persistence/project_manager.hpp"
#include "persistence/recovery.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace openphysx;
namespace fs = std::filesystem;

namespace {

void write_text(const fs::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
}

} // namespace

TEST_CASE("a dead session lock is offered for recovery and opens untitled")
{
    const fs::path root = fs::temp_directory_path() / "openphysx-recovery";
    std::error_code error;
    fs::remove_all(root, error);
    const fs::path crashed = root / "dead-session";
    fs::create_directories(crashed / "ignored");
    write_text(crashed / "session.lock", "4294967294\n0\n");
    write_text(crashed / "origin.txt", "D:/robots/MyRobot.opx\n");

    Project project;
    project.scene.CreateEntityWithUUID(0x11ull, "Recovered");
    project.created_utc = "2026-10-10T00:00:00Z";
    REQUIRE(ProjectFile::Save(crashed / "MyRobot_20261010T120000Z_1.opx", project));

    const fs::path current = root / "live";
    fs::create_directories(current);
    write_text(current / "session.lock", "1\n0\n");
    const auto found = FindCrashedSessions(root, current);
    REQUIRE(found.size() == 1);
    CHECK(found.front().name == "MyRobot.opx");
    CHECK(found.front().origin == fs::path("D:/robots/MyRobot.opx"));
    CHECK_FALSE(ProcessAlive(4294967294u));

    Simulation simulation;
    CommandStack commands;
    ProjectManager manager;
    const auto report = manager.Recover(found.front().autosave, found.front().origin, simulation, commands);
    REQUIRE(report.has_value());
    CHECK_FALSE(manager.has_path());
    CHECK(commands.IsDirty());
    CHECK(simulation.editor_scene().FindByName("Recovered"));
    CHECK(manager.RecoverHint() == fs::path("D:/robots/MyRobot.opx"));

    fs::remove_all(root, error);
}

#if defined(_WIN32)
TEST_CASE("taskkill makes a session look crashed")
{
    const fs::path root = fs::temp_directory_path() / "openphysx-taskkill";
    std::error_code error;
    fs::remove_all(root, error);
    wchar_t command[] = L"powershell.exe -NoProfile -Command Start-Sleep -Seconds 30";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        return;

    const fs::path dir = root / "killed";
    fs::create_directories(dir);
    {
        std::ofstream lock(dir / "session.lock", std::ios::trunc);
        lock << process.dwProcessId << "\n0\n";
    }
    write_text(dir / "keep_20261010T120000Z_1.opx", "{\"magic\":\"OPX\"}\n");
    const std::string kill = "taskkill /F /PID " + std::to_string(process.dwProcessId) + " >NUL 2>&1";
    std::system(kill.c_str());
    WaitForSingleObject(process.hProcess, 5000);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);

    CHECK_FALSE(ProcessAlive(process.dwProcessId));
    const auto found = FindCrashedSessions(root, root / "other");
    CHECK(found.size() == 1);
    fs::remove_all(root, error);
}
#endif
