#include "persistence/sidecar.hpp"

#include "persistence/atomic_write.hpp"
#include "persistence/checksum.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

namespace openphysx {
namespace {

constexpr char kCacheMagic[4] = {'O', 'P', 'X', 'C'};

std::string read_bytes(const std::filesystem::path& path, bool& ok)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        ok = false;
        return {};
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    ok = static_cast<bool>(input) || input.eof();
    return buffer.str();
}

} // namespace

std::filesystem::path SidecarDir(const std::filesystem::path& project)
{
    return std::filesystem::path(project.wstring() + L".data");
}

std::expected<AssetRecord, SaveError> ImportAsset(
    const std::filesystem::path& project, const std::filesystem::path& source, UUID id)
{
    bool ok = false;
    const std::string bytes = read_bytes(source, ok);
    if (!ok)
        return std::unexpected(SaveError{SaveError::Kind::Io, "The asset could not be read."});

    const std::filesystem::path destination = SidecarDir(project) / "assets" / source.filename();
    if (auto written = AtomicWrite(destination, bytes); !written)
        return std::unexpected(written.error());

    AssetRecord record;
    record.id = id;
    record.path = std::string("assets/") + source.filename().string();
    record.hash = FormatChecksum(Xxh64(bytes));
    return record;
}

std::expected<void, SaveError> WriteCache(
    const std::filesystem::path& project, std::string_view name, std::span<const float> samples)
{
    std::string bytes;
    bytes.append(kCacheMagic, 4);
    const auto count = static_cast<std::uint32_t>(samples.size());
    bytes.append(reinterpret_cast<const char*>(&count), sizeof(count));
    if (!samples.empty())
        bytes.append(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(samples.size() * sizeof(float)));
    return AtomicWrite(SidecarDir(project) / "cache" / (std::string(name) + ".bin"), bytes);
}

std::expected<std::vector<float>, LoadError> ReadCache(const std::filesystem::path& project, std::string_view name)
{
    const std::filesystem::path path = SidecarDir(project) / "cache" / (std::string(name) + ".bin");
    bool ok = false;
    const std::string bytes = read_bytes(path, ok);
    if (!ok)
        return std::unexpected(LoadError{LoadError::Kind::NotFound, "The cache is missing and can be regenerated."});
    if (bytes.size() < 8 || bytes.compare(0, 4, kCacheMagic, 4) != 0)
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, "The cache file is not an OpenPhysX cache."});

    std::uint32_t count = 0;
    std::memcpy(&count, bytes.data() + 4, sizeof(count));
    const std::size_t need = 8u + static_cast<std::size_t>(count) * sizeof(float);
    if (bytes.size() < need)
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, "The cache file is truncated."});

    std::vector<float> samples(count);
    if (count > 0)
        std::memcpy(samples.data(), bytes.data() + 8, static_cast<std::size_t>(count) * sizeof(float));
    return samples;
}

} // namespace openphysx
