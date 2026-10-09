#pragma once

#include "ecs/load_report.hpp"
#include "persistence/autosave.hpp"
#include "persistence/recovery.hpp"
#include "persistence/project_manager.hpp"
#include "persistence/recent_files.hpp"

#include <filesystem>
#include <string>

namespace openphysx {

class CommandStack;
class IRenderer;
class ISimulation;

enum class FilePending
{
    None,
    New,
    Open,
    OpenPath,
    Exit,
    Revert,
};

struct FileSession
{
    ProjectManager projects;
    RecentFiles recent;
    Autosave autosave;
    std::filesystem::path pending_path;
    FilePending pending = FilePending::None;
    bool unsaved_popup = false;
    bool error_popup = false;
    bool apply_view = false;
    bool reset_view = false;
    bool recovery_scanned = false;
    bool recovery_popup = false;
    std::vector<CrashedSession> crashed;
    std::string error;
    LoadReport load_report;
    bool load_report_open = false;
};

void InitFileDialogs();
void ShutdownFileDialogs();

void DrawFileMenu(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit);
void HandleFileShortcuts(
    FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool text_input, bool& quit);
void DrawFilePopups(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit);
void RequestFileQuit(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit);
void ApplyLoadedView(FileSession& session, IRenderer& renderer);
void UpdateWindowTitle(const FileSession& session, const CommandStack& commands);

} // namespace openphysx
