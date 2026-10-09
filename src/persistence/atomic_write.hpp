#pragma once

#include "persistence/errors.hpp"

#include <expected>
#include <filesystem>
#include <string_view>

namespace openphysx {

enum class AtomicFailPoint
{
    None,
    AfterTempWrite,
};

// Test hook. Production calls leave this at None.
void SetAtomicFailPoint(AtomicFailPoint point);

// Writes bytes to a temp file in the same directory, flushes it, moves the
// existing target to target.bak, then renames the temp file onto the target.
// A failure leaves the original target in place and deletes the temp file.
std::expected<void, SaveError> AtomicWrite(const std::filesystem::path& target, std::string_view bytes);

} // namespace openphysx
