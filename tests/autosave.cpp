#include "persistence/autosave.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace openphysx;
namespace fs = std::filesystem;

TEST_CASE("autosave writes a snapshot on a background thread and keeps five files")
{
    const fs::path dir = fs::temp_directory_path() / "openphysx-autosave";
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);

    Autosave autosave(dir);
    autosave.SetInterval(std::chrono::milliseconds(0));
    int writes = 0;
    for (int i = 0; i < 7; ++i)
    {
        autosave.Tick(
            true,
            true,
            [&] {
                ++writes;
                return "snapshot-" + std::to_string(i);
            },
            "My Robot");
        autosave.WaitIdle();
    }
    CHECK(writes == 7);

    int files = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir))
    {
        if (entry.path().extension() == ".opx")
            ++files;
    }
    CHECK(files == 5);

    autosave.Tick(true, false, [] { return std::string("nope"); }, "My Robot");
    autosave.WaitIdle();
    CHECK(writes == 7);

    autosave.DeleteAutosaves();
    files = 0;
    if (fs::exists(dir))
    {
        for (const fs::directory_entry& entry : fs::directory_iterator(dir))
        {
            if (entry.path().extension() == ".opx")
                ++files;
        }
    }
    CHECK(files == 0);
    fs::remove_all(dir, error);
}
