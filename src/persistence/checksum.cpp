#include "persistence/checksum.hpp"

#include "ecs/uuid.hpp"

#include <bit>
#include <cstring>

namespace openphysx {
namespace {

static_assert(std::endian::native == std::endian::little, "XXH64 reads bytes in little-endian order.");

constexpr std::uint64_t kPrime1 = 0x9E3779B185EBCA87ULL;
constexpr std::uint64_t kPrime2 = 0xC2B2AE3D27D4EB4FULL;
constexpr std::uint64_t kPrime3 = 0x165667B19E3779F9ULL;
constexpr std::uint64_t kPrime4 = 0x85EBCA77C2B2AE63ULL;
constexpr std::uint64_t kPrime5 = 0x27D4EB2F165667C5ULL;

std::uint64_t rotl(std::uint64_t value, int shift)
{
    return (value << shift) | (value >> (64 - shift));
}

std::uint64_t read64(const std::uint8_t* data)
{
    std::uint64_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

std::uint32_t read32(const std::uint8_t* data)
{
    std::uint32_t value = 0;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

std::uint64_t round64(std::uint64_t acc, std::uint64_t input)
{
    acc += input * kPrime2;
    acc = rotl(acc, 31);
    acc *= kPrime1;
    return acc;
}

std::uint64_t merge_round(std::uint64_t acc, std::uint64_t value)
{
    value = round64(0, value);
    acc ^= value;
    acc = acc * kPrime1 + kPrime4;
    return acc;
}

std::uint64_t avalanche(std::uint64_t hash)
{
    hash ^= hash >> 33;
    hash *= kPrime2;
    hash ^= hash >> 29;
    hash *= kPrime3;
    hash ^= hash >> 32;
    return hash;
}

} // namespace

std::uint64_t Xxh64(const void* data, std::size_t size, std::uint64_t seed)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    const std::uint8_t* cursor = bytes;
    const std::uint8_t* const end = bytes + size;
    std::uint64_t hash = 0;

    if (size >= 32)
    {
        const std::uint8_t* const limit = end - 32;
        std::uint64_t v1 = seed + kPrime1 + kPrime2;
        std::uint64_t v2 = seed + kPrime2;
        std::uint64_t v3 = seed;
        std::uint64_t v4 = seed - kPrime1;
        do
        {
            v1 = round64(v1, read64(cursor));
            cursor += 8;
            v2 = round64(v2, read64(cursor));
            cursor += 8;
            v3 = round64(v3, read64(cursor));
            cursor += 8;
            v4 = round64(v4, read64(cursor));
            cursor += 8;
        } while (cursor <= limit);

        hash = rotl(v1, 1) + rotl(v2, 7) + rotl(v3, 12) + rotl(v4, 18);
        hash = merge_round(hash, v1);
        hash = merge_round(hash, v2);
        hash = merge_round(hash, v3);
        hash = merge_round(hash, v4);
    }
    else
    {
        hash = seed + kPrime5;
    }

    hash += static_cast<std::uint64_t>(size);

    while (cursor + 8 <= end)
    {
        const std::uint64_t k1 = round64(0, read64(cursor));
        hash ^= k1;
        hash = rotl(hash, 27) * kPrime1 + kPrime4;
        cursor += 8;
    }

    if (cursor + 4 <= end)
    {
        hash ^= static_cast<std::uint64_t>(read32(cursor)) * kPrime1;
        hash = rotl(hash, 23) * kPrime2 + kPrime3;
        cursor += 4;
    }

    while (cursor < end)
    {
        hash ^= static_cast<std::uint64_t>(*cursor) * kPrime5;
        hash = rotl(hash, 11) * kPrime1;
        ++cursor;
    }

    return avalanche(hash);
}

std::uint64_t Xxh64(std::string_view text, std::uint64_t seed)
{
    return Xxh64(text.data(), text.size(), seed);
}

std::string FormatChecksum(std::uint64_t hash)
{
    return "xxh64:" + UuidToHex(hash);
}

} // namespace openphysx
