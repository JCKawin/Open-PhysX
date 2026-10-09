#include "ui/file_menu.hpp"

#include "logic/ISimulation.h"
#include "persistence/app_data.hpp"
#include "renderer/IRenderer.h"

#include "imgui.h"
#include "nfd.hpp"

#include "raylib.h"

#include <filesystem>
#include <string>

namespace openphysx {
namespace {

extern "C" void glfwSetWindowShouldClose(void* window, int value);

nfdfilteritem_t kProjectFilter[] = {{"OpenPhysX project", "opx"}};

std::filesystem::path path_from_dialog(const nfdchar_t* text)
{
    const auto* bytes = reinterpret_cast<const char8_t*>(text);
    return std::filesystem::path(std::u8string(bytes));
}

std::filesystem::path with_extension(std::filesystem::path path)
{
    if (path.extension() != ".opx")
        path += ".opx";
    return path;
}

std::string layout_text()
{
    const char* ini = ImGui::SaveIniSettingsToMemory();
    return ini == nullptr ? std::string{} : std::string(ini);
}

void show_error(FileSession& session, std::string message)
{
    session.error = std::move(message);
    session.error_popup = true;
}

bool save_as(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands)
{
    NFD::UniquePath chosen;
    const std::string suggested = session.projects.has_path() ? session.projects.DisplayName() : std::string("Untitled.opx");
    std::string hint_utf8;
    const nfdchar_t* hint = nullptr;
    if (!session.projects.RecoverHint().empty())
    {
        const std::u8string text = session.projects.RecoverHint().parent_path().u8string();
        hint_utf8.assign(reinterpret_cast<const char*>(text.data()), text.size());
        hint = hint_utf8.c_str();
    }
    const nfdresult_t result = NFD::SaveDialog(chosen, kProjectFilter, 1, hint, suggested.c_str());
    if (result == NFD_CANCEL)
        return false;
    if (result != NFD_OKAY)
    {
        show_error(session, NFD::GetError() != nullptr ? NFD::GetError() : "The save dialog could not be opened.");
        return false;
    }

    const auto saved = session.projects.SaveAs(
        with_extension(path_from_dialog(chosen.get())), simulation, commands, renderer.view(), layout_text());
    if (!saved)
    {
        show_error(session, saved.error().message);
        return false;
    }
    session.recent.Add(session.projects.path());
    session.autosave.DeleteAutosaves();
    return true;
}

bool save_project(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands)
{
    if (!session.projects.has_path())
        return save_as(session, simulation, renderer, commands);

    const auto saved = session.projects.Save(simulation, commands, renderer.view(), layout_text());
    if (!saved)
    {
        show_error(session, saved.error().message);
        return false;
    }
    session.recent.Add(session.projects.path());
    session.autosave.DeleteAutosaves();
    return true;
}

void perform(FileSession& session, FilePending action, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit)
{
    (void)renderer;
    switch (action)
    {
    case FilePending::None:
        break;
    case FilePending::New:
        session.projects.New(simulation, commands);
        session.reset_view = true;
        break;
    case FilePending::Open:
    {
        NFD::UniquePath chosen;
        const nfdresult_t result = NFD::OpenDialog(chosen, kProjectFilter, 1);
        if (result == NFD_CANCEL)
            break;
        if (result != NFD_OKAY)
        {
            show_error(session, NFD::GetError() != nullptr ? NFD::GetError() : "The open dialog could not be opened.");
            break;
        }
        const auto opened = session.projects.Open(path_from_dialog(chosen.get()), simulation, commands);
        if (!opened)
            show_error(session, opened.error().message);
        else
        {
            session.apply_view = true;
            session.recent.Add(session.projects.path());
        }
        break;
    }
    case FilePending::OpenPath:
    {
        const auto opened = session.projects.Open(session.pending_path, simulation, commands);
        if (!opened)
            show_error(session, opened.error().message);
        else
        {
            session.apply_view = true;
            session.recent.Add(session.projects.path());
        }
        break;
    }
    case FilePending::Revert:
    {
        const auto reverted = session.projects.Revert(simulation, commands);
        if (!reverted)
            show_error(session, reverted.error().message);
        else
            session.apply_view = true;
        break;
    }
    case FilePending::Exit:
        quit = true;
        break;
    }
}

void request(
    FileSession& session, FilePending action, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit)
{
    if (commands.IsDirty())
    {
        session.pending = action;
        session.unsaved_popup = true;
        return;
    }
    perform(session, action, simulation, renderer, commands, quit);
}

} // namespace

void InitFileDialogs()
{
    NFD::Init();
}

void ShutdownFileDialogs()
{
    NFD::Quit();
}

void UpdateWindowTitle(const FileSession& session, const CommandStack& commands)
{
    SetWindowTitle(session.projects.WindowTitle(commands).c_str());
}

void ApplyLoadedView(FileSession& session, IRenderer& renderer)
{
    if (session.reset_view)
    {
        renderer.reset_view();
        session.reset_view = false;
        session.apply_view = false;
        return;
    }
    if (!session.apply_view)
        return;

    View3D& view = renderer.view();
    const ProjectCamera& camera = session.projects.camera();
    view.viewquat = camera.rotation;
    view.ofs = camera.offset;
    view.dist = camera.distance;
    view.fovy_deg = camera.fovy_deg;
    view.projection = camera.orthographic ? ViewProjection::Orthographic : ViewProjection::Perspective;
    if (!session.projects.layout_ini().empty())
        ImGui::LoadIniSettingsFromMemory(session.projects.layout_ini().c_str());
    session.apply_view = false;
}

void DrawFileMenu(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit)
{
    if (!ImGui::BeginMenu("File"))
        return;

    if (ImGui::MenuItem("New", "Ctrl+N"))
        request(session, FilePending::New, simulation, renderer, commands, quit);
    if (ImGui::MenuItem("Open...", "Ctrl+O"))
        request(session, FilePending::Open, simulation, renderer, commands, quit);
    if (ImGui::BeginMenu("Open Recent"))
    {
        if (session.recent.Entries().empty())
            ImGui::MenuItem("(Empty)", nullptr, false, false);
        for (const std::filesystem::path& entry : session.recent.Entries())
        {
            const std::u8string text = entry.u8string();
            const std::string label(text.begin(), text.end());
            const bool exists = RecentFiles::Exists(entry);
            if (ImGui::MenuItem(label.c_str(), nullptr, false, exists))
            {
                session.pending_path = entry;
                request(session, FilePending::OpenPath, simulation, renderer, commands, quit);
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S"))
        save_project(session, simulation, renderer, commands);
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
        save_as(session, simulation, renderer, commands);
    if (ImGui::MenuItem("Revert", nullptr, false, session.projects.has_path()))
        request(session, FilePending::Revert, simulation, renderer, commands, quit);
    if (ImGui::MenuItem("Recover Autosave...", nullptr, false, !session.crashed.empty()))
        session.recovery_popup = true;
    ImGui::Separator();
    if (ImGui::MenuItem("Exit", "Ctrl+Q"))
        request(session, FilePending::Exit, simulation, renderer, commands, quit);
    ImGui::EndMenu();
}

void HandleFileShortcuts(
    FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool text_input, bool& quit)
{
    if (text_input)
        return;

    const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (!ctrl)
        return;

    if (shift && IsKeyPressed(KEY_S))
        save_as(session, simulation, renderer, commands);
    else if (IsKeyPressed(KEY_S))
        save_project(session, simulation, renderer, commands);
    else if (IsKeyPressed(KEY_N))
        request(session, FilePending::New, simulation, renderer, commands, quit);
    else if (IsKeyPressed(KEY_O))
        request(session, FilePending::Open, simulation, renderer, commands, quit);
    else if (IsKeyPressed(KEY_Q))
        request(session, FilePending::Exit, simulation, renderer, commands, quit);
}

void DrawFilePopups(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit)
{
    if (!session.recovery_scanned)
    {
        session.crashed = FindCrashedSessions(AppDataDir() / "autosave", session.autosave.SessionDir());
        session.recovery_scanned = true;
        session.recovery_popup = !session.crashed.empty();
    }
    if (session.recovery_popup && !session.crashed.empty())
        ImGui::OpenPopup("Recover Autosave");
    if (ImGui::BeginPopupModal("Recover Autosave", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const CrashedSession& crashed = session.crashed.front();
        ImGui::Text("OpenPhysX closed unexpectedly. Recover %s?", crashed.name.c_str());
        ImGui::TextWrapped("%s", crashed.when.c_str());
        if (session.crashed.size() > 1)
            ImGui::Text("Other sessions: %d", static_cast<int>(session.crashed.size() - 1));
        if (ImGui::Button("Recover"))
        {
            const auto recovered = session.projects.Recover(crashed.autosave, crashed.origin, simulation, commands);
            std::error_code error;
            std::filesystem::remove_all(crashed.directory, error);
            session.crashed.erase(session.crashed.begin());
            if (!recovered)
                show_error(session, recovered.error().message);
            else
                session.apply_view = true;
            if (session.crashed.empty())
            {
                session.recovery_popup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard"))
        {
            std::error_code error;
            std::filesystem::remove_all(session.crashed.front().directory, error);
            session.crashed.erase(session.crashed.begin());
            if (session.crashed.empty())
            {
                session.recovery_popup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Decide Later"))
        {
            session.recovery_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }


    if (session.unsaved_popup)
    {
        ImGui::OpenPopup("Unsaved Changes");
        session.unsaved_popup = false;
    }
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Save changes to the current project?");
        if (!session.error.empty())
            ImGui::TextWrapped("%s", session.error.c_str());
        if (ImGui::Button("Save"))
        {
            if (save_project(session, simulation, renderer, commands))
            {
                const FilePending next = session.pending;
                session.pending = FilePending::None;
                session.error.clear();
                ImGui::CloseCurrentPopup();
                perform(session, next, simulation, renderer, commands, quit);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Don't Save"))
        {
            const FilePending next = session.pending;
            session.pending = FilePending::None;
            session.error.clear();
            ImGui::CloseCurrentPopup();
            perform(session, next, simulation, renderer, commands, quit);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            session.pending = FilePending::None;
            session.error.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (session.error_popup)
    {
        ImGui::OpenPopup("Project Error");
        session.error_popup = false;
    }
    if (ImGui::BeginPopupModal("Project Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("%s", session.error.c_str());
        if (ImGui::Button("Save As..."))
        {
            ImGui::CloseCurrentPopup();
            save_as(session, simulation, renderer, commands);
        }
        ImGui::SameLine();
        if (ImGui::Button("OK"))
        {
            session.error.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void RequestFileQuit(FileSession& session, ISimulation& simulation, IRenderer& renderer, CommandStack& commands, bool& quit)
{
    glfwSetWindowShouldClose(GetWindowHandle(), 0);
    request(session, FilePending::Exit, simulation, renderer, commands, quit);
}

} // namespace openphysx
