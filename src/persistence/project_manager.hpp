#pragma once

#include "core/View.h"
#include "editor/command.hpp"
#include "persistence/migrations.hpp"
#include "persistence/project_file.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace openphysx {

// Path, timestamps, and the bridge between the live editor scene and the project file.
// Dirty state is the command stack revision, not a separate flag.
class ProjectManager
{
public:
    bool has_path() const { return !path_.empty(); }
    const std::filesystem::path& path() const { return path_; }
    const std::filesystem::path& RecoverHint() const { return recover_hint_; }
    const ProjectCamera& camera() const { return camera_; }
    const std::string& layout_ini() const { return layout_ini_; }
    Vec3 gravity() const { return gravity_; }
    float timestep() const { return timestep_; }

    std::string DisplayName() const;
    std::string WindowTitle(const CommandStack& commands) const;

    void New(ISimulation& simulation, CommandStack& commands);
    std::expected<LoadReport, LoadError> Open(const std::filesystem::path& path, ISimulation& simulation, CommandStack& commands);
    std::expected<LoadReport, LoadError> Recover(
        const std::filesystem::path& autosave, const std::filesystem::path& hint, ISimulation& simulation, CommandStack& commands);
    std::expected<LoadReport, LoadError> Revert(ISimulation& simulation, CommandStack& commands);
    std::expected<std::string, SaveError> Snapshot(const ISimulation& simulation, const View3D& view, std::string layout) const;
    std::expected<void, SaveError> Save(ISimulation& simulation, CommandStack& commands, const View3D& view, std::string layout);
    std::expected<void, SaveError> SaveAs(
        const std::filesystem::path& path, ISimulation& simulation, CommandStack& commands, const View3D& view, std::string layout);

private:
    Project capture(const ISimulation& simulation, const View3D& view, std::string layout) const;
    void apply(ISimulation& simulation, Project project, CommandStack& commands);

    std::filesystem::path path_;
    std::filesystem::path recover_hint_;
    std::string created_utc_;
    ProjectCamera camera_{};
    std::string layout_ini_;
    Vec3 gravity_{0.0f, -9.81f, 0.0f};
    float timestep_ = 0.001f;
    int source_version_ = Project::kFormatVersion;
    bool archived_migration_ = true;
};

} // namespace openphysx
