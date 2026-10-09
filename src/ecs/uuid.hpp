#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace openphysx {

// Random 64-bit id. Zero is null and is never generated.
using UUID = std::uint64_t;

inline constexpr UUID kNullUuid = 0;

UUID GenerateUuid();

std::string UuidToHex(UUID id);
std::optional<UUID> UuidFromHex(std::string_view text);

} // namespace openphysx
