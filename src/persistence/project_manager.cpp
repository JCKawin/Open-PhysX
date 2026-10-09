#include "persistence/project_manager.hpp"

#include "ecs/pose.hpp"

#include <algorithm>

namespace openphysx {
namespace {

bool listed(const std::vector<UUID>& ids, UUID id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

} // namespace

std::string ProjectManager::DisplayName() const
{
    if (path_.empty())
        return "Untitled";
    const std::u8string name = path_.filename().u8string();
    return std::string(name.begin(), name.end());
}

std::string ProjectManager::WindowTitle(const CommandStack& commands) const
{
    std::string title = "OpenPhysX - " + DisplayName();
    if (commands.IsDirty())
        title.push_back('*');
    return title;
}

Project ProjectManager::capture(const ISimulation& simulation, const View3D& view, std::string layout) const
{
    Project project;
    project.scene = simulation.editor_scene();
    project.gravity = gravity_;
    project.timestep = timestep_;
    project.duration = simulation.state().duration;
    project.playback_speed = simulation.state().playback_speed;
    project.loop = simulation.state().loop;
    project.show_grid = simulation.state().show_grid;
    project.demo_motion = simulation.state().demo_motion;
    project.grid_slices = simulation.state().grid_slices;
    project.grid_spacing = simulation.state().grid_spacing;
    project.clear_color = simulation.state().clear_color;
    project.camera.rotation = view.viewquat;
    project.camera.offset = view.ofs;
    project.camera.distance = view.dist;
    project.camera.fovy_deg = view.fovy_deg;
    project.camera.orthographic = view.projection == ViewProjection::Orthographic;
    project.layout_ini = std::move(layout);
    project.created_utc = created_utc_;

    if (simulation.active_id() != kNullUuid)
        project.selection.push_back(simulation.active_id());
    for (const UUID id : simulation.editor_scene().CreationOrder())
    {
        if (id == simulation.active_id())
            continue;
        const Entity entity = simulation.editor_scene().FindByUUID(id);
        if (entity && EntitySelected(entity))
            project.selection.push_back(id);
    }
    return project;
}

void ProjectManager::apply(ISimulation& simulation, Project project, CommandStack& commands)
{
    const std::vector<UUID> selection = project.selection;
    simulation.stop();
    simulation.state() = SimulationState{};
    simulation.state().duration = project.duration;
    simulation.state().playback_speed = project.playback_speed;
    simulation.state().loop = project.loop;
    simulation.state().show_grid = project.show_grid;
    simulation.state().demo_motion = project.demo_motion;
    simulation.state().grid_slices = project.grid_slices;
    simulation.state().grid_spacing = project.grid_spacing;
    simulation.state().clear_color = project.clear_color;
    simulation.editor_scene() = std::move(project.scene);

    for (const UUID id : simulation.editor_scene().CreationOrder())
    {
        Entity entity = simulation.editor_scene().FindByUUID(id);
        if (!entity)
            continue;
        if (listed(selection, id))
            SetEntitySelected(entity, true);
        else if (entity.Has<SelectionOutlineTag>())
            entity.Remove<SelectionOutlineTag>();
    }

    UUID active = selection.empty() ? kNullUuid : selection.front();
    if (active == kNullUuid || !simulation.editor_scene().FindByUUID(active))
        active = simulation.editor_scene().CreationOrder().empty() ? kNullUuid : simulation.editor_scene().CreationOrder().front();
    simulation.set_active(active);
    if (Entity active_entity = simulation.editor_scene().FindByUUID(active))
        SetEntitySelected(active_entity, true);

    gravity_ = project.gravity;
    timestep_ = project.timestep;
    camera_ = project.camera;
    layout_ini_ = std::move(project.layout_ini);
    created_utc_ = std::move(project.created_utc);
    commands.Clear();
}

void ProjectManager::New(ISimulation& simulation, CommandStack& commands)
{
    simulation.reset();
    commands.Clear();
    path_.clear();
    created_utc_.clear();
    camera_ = {};
    layout_ini_.clear();
    gravity_ = {0.0f, -9.81f, 0.0f};
    timestep_ = 0.001f;
}

std::expected<LoadReport, LoadError> ProjectManager::Open(
    const std::filesystem::path& path, ISimulation& simulation, CommandStack& commands)
{
    Project loaded;
    auto report = ProjectFile::Load(path, loaded);
    if (!report)
        return std::unexpected(report.error());
    apply(simulation, std::move(loaded), commands);
    path_ = path;
    return std::move(report.value());
}

std::expected<LoadReport, LoadError> ProjectManager::Revert(ISimulation& simulation, CommandStack& commands)
{
    if (path_.empty())
        return std::unexpected(LoadError{LoadError::Kind::NotFound, "This project has not been saved."});
    return Open(path_, simulation, commands);
}

std::expected<void, SaveError> ProjectManager::Save(
    ISimulation& simulation, CommandStack& commands, const View3D& view, std::string layout)
{
    if (path_.empty())
        return std::unexpected(SaveError{SaveError::Kind::Serialize, "Choose a file name before saving."});
    if (created_utc_.empty())
        created_utc_ = CurrentUtcTimestamp();

    const Project project = capture(simulation, view, std::move(layout));
    if (auto saved = ProjectFile::Save(path_, project); !saved)
        return saved;
    commands.MarkSaved();
    return {};
}

std::expected<void, SaveError> ProjectManager::SaveAs(
    const std::filesystem::path& path, ISimulation& simulation, CommandStack& commands, const View3D& view, std::string layout)
{
    const std::filesystem::path previous = path_;
    path_ = path;
    auto saved = Save(simulation, commands, view, std::move(layout));
    if (!saved)
        path_ = previous;
    return saved;
}

} // namespace openphysx
