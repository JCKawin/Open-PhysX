#include "persistence/app_data.hpp"

#include <cstdlib>

namespace openphysx {

std::filesystem::path AppDataDir()
{
#if defined(_WIN32)
    const char* root = std::getenv("APPDATA");
    const std::filesystem::path base = root != nullptr ? std::filesystem::path(root) : std::filesystem::temp_directory_path();
    return base / "OpenPhysX";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    const std::filesystem::path base = home != nullptr ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
    return base / "Library" / "Application Support" / "OpenPhysX";
#else
    const char* home = std::getenv("HOME");
    const std::filesystem::path base = home != nullptr ? std::filesystem::path(home) : std::filesystem::temp_directory_path();
    return base / ".config" / "OpenPhysX";
#endif
}

} // namespace openphysx
