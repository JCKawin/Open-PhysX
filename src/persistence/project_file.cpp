#include "persistence/project_file.hpp"

#include "ecs/components/serialize.hpp"
#include "persistence/atomic_write.hpp"
#include "persistence/checksum.hpp"
#include "persistence/migrations.hpp"
#include "persistence/scene_serializer.hpp"

#include <ctime>
#include <fstream>
#include <sstream>

namespace openphysx {

std::string CurrentUtcTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm time_parts{};
#if defined(_WIN32)
    gmtime_s(&time_parts, &now);
#else
    gmtime_r(&now, &time_parts);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &time_parts);
    return buffer;
}

namespace {

bool too_deep(const nlohmann::json& json, int depth)
{
    if (depth > 64)
        return true;
    if (json.is_array())
    {
        for (const nlohmann::json& item : json)
        {
            if (too_deep(item, depth + 1))
                return true;
        }
    }
    else if (json.is_object())
    {
        for (const auto& item : json.items())
        {
            if (too_deep(item.value(), depth + 1))
                return true;
        }
    }
    return false;
}

std::string payload_checksum(const nlohmann::json& document)
{
    nlohmann::json copy = document;
    copy.erase("checksum");
    copy.erase("modified_utc");
    return FormatChecksum(Xxh64(copy.dump()));
}

nlohmann::json camera_json(const ProjectCamera& camera)
{
    return nlohmann::json{
        {"rotation", camera.rotation},
        {"offset", camera.offset},
        {"distance", camera.distance},
        {"fovy_deg", camera.fovy_deg},
        {"orthographic", camera.orthographic},
    };
}

ProjectCamera camera_from_json(const nlohmann::json& json)
{
    ProjectCamera camera;
    if (!json.is_object())
        return camera;
    camera.rotation = json.value("rotation", camera.rotation);
    camera.offset = json.value("offset", camera.offset);
    camera.distance = json.value("distance", camera.distance);
    camera.fovy_deg = json.value("fovy_deg", camera.fovy_deg);
    camera.orthographic = json.value("orthographic", camera.orthographic);
    return camera;
}

nlohmann::json project_json(const Project& project, const std::string& created, const std::string& modified)
{
    nlohmann::json selection = nlohmann::json::array();
    for (const UUID id : project.selection)
        selection.push_back(UuidToHex(id));

    nlohmann::json assets = nlohmann::json::array();
    for (const AssetRecord& asset : project.assets)
    {
        assets.push_back(nlohmann::json{
            {"id", UuidToHex(asset.id)},
            {"path", asset.path},
            {"hash", asset.hash},
        });
    }

    return nlohmann::json{
        {"magic", "OPX"},
        {"format_version", Project::kFormatVersion},
        {"app_version", Project::kAppVersion},
        {"created_utc", created},
        {"modified_utc", modified},
        {"units", {{"length", "m"}, {"mass", "kg"}, {"time", "s"}, {"angle", "rad"}}},
        {"scene", WriteScene(project.scene)},
        {"simulation",
         {{"gravity", project.gravity},
          {"timestep", project.timestep},
          {"duration", project.duration},
          {"playback_speed", project.playback_speed},
          {"loop", project.loop},
          {"show_grid", project.show_grid},
          {"demo_motion", project.demo_motion},
          {"grid_slices", project.grid_slices},
          {"grid_spacing", project.grid_spacing},
          {"clear_color", project.clear_color}}},
        {"editor",
         {{"camera", camera_json(project.camera)}, {"selection", std::move(selection)}, {"layout_ini", project.layout_ini}}},
        {"assets", std::move(assets)},
    };
}

std::expected<std::string, LoadError> read_text(const std::filesystem::path& path)
{
    std::error_code error;
    if (!std::filesystem::exists(path, error) || error)
        return std::unexpected(LoadError{LoadError::Kind::NotFound, "The project file was not found."});

    const auto size = std::filesystem::file_size(path, error);
    if (error)
        return std::unexpected(LoadError{LoadError::Kind::Io, "The project file could not be read."});
    if (size > Project::kMaxFileBytes)
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, "The project file is larger than OpenPhysX will open."});

    std::ifstream input(path, std::ios::binary);
    if (!input)
        return std::unexpected(LoadError{LoadError::Kind::Io, "The project file could not be read."});
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input && !input.eof())
        return std::unexpected(LoadError{LoadError::Kind::Io, "The project file could not be read."});
    return buffer.str();
}

} // namespace

std::expected<void, SaveError> ProjectFile::Save(const std::filesystem::path& path, const Project& project)
{
    try
    {
        const std::string modified = CurrentUtcTimestamp();
        const std::string created = project.created_utc.empty() ? modified : project.created_utc;
        nlohmann::json document = project_json(project, created, modified);
        document["checksum"] = payload_checksum(document);
        std::string text = document.dump(2);
        text.push_back('\n');
        return AtomicWrite(path, text);
    }
    catch (const std::exception&)
    {
        return std::unexpected(SaveError{SaveError::Kind::Serialize, "The project could not be serialized."});
    }
}

std::expected<LoadReport, LoadError> ProjectFile::Load(const std::filesystem::path& path, Project& out)
{
    const auto text = read_text(path);
    if (!text)
        return std::unexpected(text.error());
    if (text->empty())
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, "This file may be corrupt. It is empty."});

    nlohmann::json document;
    try
    {
        document = nlohmann::json::parse(*text);
    }
    catch (const nlohmann::json::exception&)
    {
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, "This file may be corrupt. It is not valid JSON."});
    }

    if (!document.is_object() || document.value("magic", std::string{}) != "OPX")
        return std::unexpected(LoadError{LoadError::Kind::Schema, "This is not an OpenPhysX project."});
    if (too_deep(document, 0))
        return std::unexpected(LoadError{LoadError::Kind::Schema, "This file is nested too deeply to open."});

    int source_version = 0;
    auto migrated = MigrateProject(std::move(document), source_version);
    if (!migrated)
        return std::unexpected(migrated.error());
    document = std::move(migrated.value());
    const int version = document.value("format_version", 0);

    const std::string checksum = document.value("checksum", std::string{});
    std::string expected;
    try
    {
        expected = payload_checksum(document);
    }
    catch (const std::exception&)
    {
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, "This file may be corrupt."});
    }
    if (checksum != expected)
    {
        const std::filesystem::path backup = path.wstring() + L".bak";
        std::error_code error;
        const bool has_backup = std::filesystem::exists(backup, error) && !error;
        std::string message = "This file may be corrupt. The checksum does not match.";
        if (has_backup)
            message += " A backup is available.";
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, std::move(message)});
    }

    Project loaded;
    loaded.format_version = version;
    loaded.source_version = source_version;
    loaded.created_utc = document.value("created_utc", std::string{});
    loaded.modified_utc = document.value("modified_utc", std::string{});

    const nlohmann::json simulation = document.value("simulation", nlohmann::json::object());
    if (simulation.is_object())
    {
        loaded.gravity = simulation.value("gravity", loaded.gravity);
        loaded.timestep = simulation.value("timestep", loaded.timestep);
        loaded.duration = simulation.value("duration", loaded.duration);
        loaded.playback_speed = simulation.value("playback_speed", loaded.playback_speed);
        loaded.loop = simulation.value("loop", loaded.loop);
        loaded.show_grid = simulation.value("show_grid", loaded.show_grid);
        loaded.demo_motion = simulation.value("demo_motion", loaded.demo_motion);
        loaded.grid_slices = simulation.value("grid_slices", loaded.grid_slices);
        loaded.grid_spacing = simulation.value("grid_spacing", loaded.grid_spacing);
        loaded.clear_color = simulation.value("clear_color", loaded.clear_color);
    }

    const nlohmann::json editor = document.value("editor", nlohmann::json::object());
    if (editor.is_object())
    {
        loaded.camera = camera_from_json(editor.value("camera", nlohmann::json::object()));
        loaded.layout_ini = editor.value("layout_ini", std::string{});
        if (editor.contains("selection") && editor.at("selection").is_array())
        {
            for (const nlohmann::json& item : editor.at("selection"))
            {
                if (!item.is_string())
                    continue;
                if (const std::optional<UUID> id = UuidFromHex(item.get<std::string>()))
                    loaded.selection.push_back(*id);
            }
        }
    }

    if (document.contains("assets") && document.at("assets").is_array())
    {
        for (const nlohmann::json& item : document.at("assets"))
        {
            if (!item.is_object())
                continue;
            AssetRecord asset;
            if (item.contains("id") && item.at("id").is_string())
                asset.id = UuidFromHex(item.at("id").get<std::string>()).value_or(kNullUuid);
            asset.path = item.value("path", std::string{});
            asset.hash = item.value("hash", std::string{});
            loaded.assets.push_back(std::move(asset));
        }
    }

    LoadReport report;
    if (!document.contains("scene") || !document.at("scene").is_object())
        return std::unexpected(LoadError{LoadError::Kind::Schema, "The project has no scene."});
    if (auto scene = ReadScene(document.at("scene"), loaded.scene, report); !scene)
        return std::unexpected(scene.error());

    out = std::move(loaded);
    return report;
}

} // namespace openphysx
