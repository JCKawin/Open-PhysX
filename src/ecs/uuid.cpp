#include "ecs/uuid.hpp"

#include <chrono>
#include <random>

namespace openphysx {
namespace {

std::mt19937_64& uuid_rng()
{
    static std::mt19937_64 rng = [] {
        const auto ticks = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::seed_seq seed{
            static_cast<std::uint32_t>(ticks),
            static_cast<std::uint32_t>(ticks >> 32),
            0x4f505800u,
            0xA11CEu,
        };
        return std::mt19937_64{seed};
    }();
    return rng;
}

int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

} // namespace

UUID GenerateUuid()
{
    UUID id = 0;
    while (id == kNullUuid)
        id = uuid_rng()();
    return id;
}

std::string UuidToHex(UUID id)
{
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string text(16, '0');
    for (int i = 15; i >= 0; --i)
    {
        text[static_cast<std::size_t>(i)] = kDigits[id & 0x0f];
        id >>= 4;
    }
    return text;
}

std::optional<UUID> UuidFromHex(std::string_view text)
{
    if (text.size() != 16)
        return std::nullopt;
    UUID id = 0;
    for (char c : text)
    {
        const int value = hex_value(c);
        if (value < 0)
            return std::nullopt;
        id = (id << 4) | static_cast<UUID>(value);
    }
    return id;
}

} // namespace openphysx
