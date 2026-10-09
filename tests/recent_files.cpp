#include "persistence/recent_files.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

using namespace openphysx;
namespace fs = std::filesystem;

TEST_CASE("recent files keep the newest ten and remember missing ones")
{
    const fs::path dir = fs::temp_directory_path() / "openphysx-recent";
    std::error_code error;
    fs::remove_all(dir, error);
    fs::create_directories(dir);
    const fs::path store = dir / "recent.txt";

    RecentFiles recent(store);
    for (int i = 0; i < 12; ++i)
    {
        const fs::path file = dir / ("file-" + std::to_string(i) + ".opx");
        std::ofstream output(file);
        output << "x";
        recent.Add(file);
    }

    CHECK(recent.Entries().size() == 10);
    CHECK(recent.Entries().front().filename() == "file-11.opx");
    CHECK(recent.Entries().back().filename() == "file-2.opx");

    RecentFiles loaded(store);
    loaded.Load();
    CHECK(loaded.Entries().size() == 10);
    CHECK(loaded.Entries().front().filename() == "file-11.opx");
    CHECK(RecentFiles::Exists(loaded.Entries().front()));

    fs::remove(dir / "file-11.opx", error);
    CHECK_FALSE(RecentFiles::Exists(loaded.Entries().front()));
    CHECK(loaded.Entries().size() == 10);

    fs::remove_all(dir, error);
}
