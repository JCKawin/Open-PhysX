#pragma once

#include <filesystem>

namespace openphysx {

// %APPDATA%/OpenPhysX, ~/.config/OpenPhysX, or ~/Library/Application Support/OpenPhysX.
std::filesystem::path AppDataDir();

} // namespace openphysx
