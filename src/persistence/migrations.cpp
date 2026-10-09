#include "persistence/migrations.hpp"

namespace openphysx {
namespace {

nlohmann::json identity(nlohmann::json document)
{
    return document;
}

// Add v1->v2 here when the schema breaks. Each step receives the previous version's tree.
using Migrator = nlohmann::json (*)(nlohmann::json);
constexpr Migrator kSteps[] = {identity};

} // namespace

std::expected<nlohmann::json, LoadError> MigrateProject(nlohmann::json document, int& source_version)
{
    if (!document.is_object())
        return std::unexpected(LoadError{LoadError::Kind::Schema, "This project file has no JSON object."});

    source_version = document.value("format_version", 0);
    if (source_version > Project::kFormatVersion)
    {
        return std::unexpected(LoadError{
            LoadError::Kind::TooNew,
            "This project was created by a newer version of OpenPhysX.",
        });
    }
    if (source_version <= 0)
        return std::unexpected(LoadError{LoadError::Kind::Schema, "This project uses a format OpenPhysX does not understand."});

    nlohmann::json current = std::move(document);
    for (int version = source_version; version < Project::kFormatVersion; ++version)
    {
        const int index = version - 1;
        if (index < 0 || index >= static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0])))
            return std::unexpected(LoadError{LoadError::Kind::Schema, "This project uses a format OpenPhysX does not understand."});
        current = kSteps[index](std::move(current));
        current["format_version"] = version + 1;
    }
    return current;
}

std::expected<void, SaveError> ArchiveMigratedOriginal(const std::filesystem::path& path, int old_version)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error) || error)
        return {};

    const std::filesystem::path archive = path.parent_path() / (path.stem().string() + ".v" + std::to_string(old_version) + ".bak");
    std::filesystem::copy_file(path, archive, std::filesystem::copy_options::overwrite_existing, error);
    if (error)
        return std::unexpected(SaveError{SaveError::Kind::Io, "The previous project version could not be kept."});
    return {};
}

} // namespace openphysx
