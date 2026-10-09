#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace openphysx {

// XXH64, seed 0 unless the caller passes another. Little-endian hosts only.
std::uint64_t Xxh64(const void* data, std::size_t size, std::uint64_t seed = 0);
std::uint64_t Xxh64(std::string_view text, std::uint64_t seed = 0);

// "xxh64:" plus 16 lowercase hex digits.
std::string FormatChecksum(std::uint64_t hash);

} // namespace openphysx
