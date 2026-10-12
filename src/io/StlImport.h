#pragma once
#include "io/Import.h"
#include <filesystem>
namespace openphysx {
ImportResult import_stl(const std::filesystem::path& path, const ImportOptions& opts);
} // namespace openphysx
