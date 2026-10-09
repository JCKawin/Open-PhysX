#include "ui/Workspace.h"

#include "ecs/pose.hpp"
#include "ui/file_menu.hpp"
#include "logic/ISimulation.h"
#include "renderer/IRenderer.h"

#include "imgui_internal.h"
#include "rlImGui.h"

#include "raylib.h"

#include <algorithm>
#include <cstdio>

namespace openphysx {
namespace {

constexpr const char* kViewportTitle = "Viewport";
constexpr const char* kPropertiesTitle = "Properties";
constexpr const char* kAnimationTitle = "Animation Player";
constexpr const char* kToolsTitle = "Tools";
constexpr const char* kDockspaceWindow = "##WorkspaceHost";
constexpr const char* kDockspaceId = "OpenPhysXDockSpace";

ImVec4 rgba(int r, int g, int b, int a = 255)
{
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void reset_simulation(Editor& editor, ISimulation& simulation)
{
    EditSnapshot before;
    before.state = simulation.state();
    before.scene = simulation.editor_scene();
    before.active = simulation.active_id();
    simulation.reset();
    editor.commit_edit(simulation, before, true);
}

} // namespace

void Workspace::init()
{
    apply_theme();
    InitFileDialogs();
    files_.recent.Load();
}

void Workspace::shutdown()
{
    files_.autosave.DeleteAutosaves();
    ShutdownFileDialogs();
}

void Workspace::request_quit(ISimulation& simulation, IRenderer& renderer)
{
    RequestFileQuit(files_, simulation, renderer, editor_.commands(), quit_requested_);
}

void Workspace::apply_theme()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;
    style.TabBarBorderSize = 1.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.DockingSeparatorSize = 3.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = rgba(232, 232, 232);
    c[ImGuiCol_TextDisabled] = rgba(140, 140, 140);
    c[ImGuiCol_WindowBg] = rgba(48, 48, 48);
    c[ImGuiCol_ChildBg] = rgba(48, 48, 48);
    c[ImGuiCol_PopupBg] = rgba(36, 36, 36, 250);
    c[ImGuiCol_Border] = rgba(28, 28, 28);
    c[ImGuiCol_BorderShadow] = rgba(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = rgba(30, 30, 30);
    c[ImGuiCol_FrameBgHovered] = rgba(54, 54, 54);
    c[ImGuiCol_FrameBgActive] = rgba(64, 64, 64);
    c[ImGuiCol_TitleBg] = rgba(29, 29, 29);
    c[ImGuiCol_TitleBgActive] = rgba(29, 29, 29);
    c[ImGuiCol_TitleBgCollapsed] = rgba(29, 29, 29);
    c[ImGuiCol_MenuBarBg] = rgba(29, 29, 29);
    c[ImGuiCol_ScrollbarBg] = rgba(36, 36, 36);
    c[ImGuiCol_ScrollbarGrab] = rgba(72, 72, 72);
    c[ImGuiCol_ScrollbarGrabHovered] = rgba(96, 96, 96);
    c[ImGuiCol_ScrollbarGrabActive] = rgba(118, 118, 118);
    c[ImGuiCol_CheckMark] = rgba(232, 158, 62);
    c[ImGuiCol_SliderGrab] = rgba(140, 140, 140);
    c[ImGuiCol_SliderGrabActive] = rgba(232, 158, 62);
    c[ImGuiCol_Button] = rgba(70, 70, 70);
    c[ImGuiCol_ButtonHovered] = rgba(92, 92, 92);
    c[ImGuiCol_ButtonActive] = rgba(232, 158, 62);
    c[ImGuiCol_Header] = rgba(62, 62, 62);
    c[ImGuiCol_HeaderHovered] = rgba(82, 82, 82);
    c[ImGuiCol_HeaderActive] = rgba(96, 96, 96);
    c[ImGuiCol_Separator] = rgba(24, 24, 24);
    c[ImGuiCol_SeparatorHovered] = rgba(232, 158, 62);
    c[ImGuiCol_SeparatorActive] = rgba(232, 158, 62);
    c[ImGuiCol_ResizeGrip] = rgba(80, 80, 80, 80);
    c[ImGuiCol_ResizeGripHovered] = rgba(232, 158, 62, 180);
    c[ImGuiCol_ResizeGripActive] = rgba(232, 158, 62);
    c[ImGuiCol_TabHovered] = rgba(72, 72, 72);
    c[ImGuiCol_Tab] = rgba(36, 36, 36);
    c[ImGuiCol_TabSelected] = rgba(56, 56, 56);
    c[ImGuiCol_TabSelectedOverline] = rgba(232, 158, 62);
    c[ImGuiCol_TabDimmed] = rgba(32, 32, 32);
    c[ImGuiCol_TabDimmedSelected] = rgba(48, 48, 48);
    c[ImGuiCol_DockingPreview] = rgba(232, 158, 62, 90);
    c[ImGuiCol_DockingEmptyBg] = rgba(36, 36, 36);
    c[ImGuiCol_TableHeaderBg] = rgba(40, 40, 40);
    c[ImGuiCol_TableBorderStrong] = rgba(24, 24, 24);
    c[ImGuiCol_TableBorderLight] = rgba(40, 40, 40);
    c[ImGuiCol_TextSelectedBg] = rgba(232, 158, 62, 80);
    c[ImGuiCol_NavHighlight] = rgba(232, 158, 62);
}

void Workspace::draw(ISimulation& simulation, IRenderer& renderer)
{
    editor_.begin_frame(simulation);
    const ImGuiIO& io = ImGui::GetIO();
    const bool idle = !io.WantTextInput && !ImGui::IsAnyItemActive() && !editor_.modal_active();
    files_.autosave.Tick(
        editor_.commands().IsDirty(),
        idle,
        [&] {
            const char* ini = ImGui::SaveIniSettingsToMemory();
            const auto bytes = files_.projects.Snapshot(simulation, renderer.view(), ini != nullptr ? ini : "");
            return bytes ? *bytes : std::string{};
        },
        files_.projects.DisplayName());
    if (files_.autosave.ConsumeNotice())
        autosave_left_ = 3.0f;
    HandleFileShortcuts(files_, simulation, renderer, editor_.commands(), io.WantTextInput, quit_requested_);
    editor_.handle_app(simulation, io.WantTextInput, ImGui::IsAnyItemFocused(), quit_requested_);
    UpdateWindowTitle(files_, editor_.commands());

    draw_menu_bar(simulation, renderer);
    draw_dockspace();
    view_ticked_ = false;
    draw_viewport(simulation, renderer);
    if (!view_ticked_)
        renderer.tick_view(GetFrameTime());
    draw_properties(simulation, renderer);
    draw_animation_player(simulation);
    draw_tools(simulation, renderer);

    if (show_demo_)
        ImGui::ShowDemoWindow(&show_demo_);
    if (show_metrics_)
        ImGui::ShowMetricsWindow(&show_metrics_);

    DrawFilePopups(files_, simulation, renderer, editor_.commands(), quit_requested_);
    ApplyLoadedView(files_, renderer);
}

void Workspace::draw_menu_bar(ISimulation& simulation, IRenderer& renderer)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    DrawFileMenu(files_, simulation, renderer, editor_.commands(), quit_requested_);

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editor_.can_undo()))
            editor_.undo(simulation);
        if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, editor_.can_redo()))
            editor_.redo(simulation);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Simulation"))
            reset_simulation(editor_, simulation);
        if (ImGui::MenuItem("Reset Camera"))
            renderer.reset_view();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Viewport", nullptr, &show_viewport_);
        ImGui::MenuItem("Properties", nullptr, &show_properties_);
        ImGui::MenuItem("Animation Player", nullptr, &show_animation_);
        ImGui::MenuItem("Tools", nullptr, &show_tools_);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout"))
        {
            show_viewport_ = show_properties_ = show_animation_ = show_tools_ = true;
            reset_layout_ = true;
        }
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &show_demo_);
        ImGui::MenuItem("ImGui Metrics", nullptr, &show_metrics_);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Settings"))
    {
        if (ImGui::MenuItem("VSync", nullptr, vsync_))
        {
            vsync_ = !vsync_;
            if (vsync_)
                SetWindowState(FLAG_VSYNC_HINT);
            else
                ClearWindowState(FLAG_VSYNC_HINT);
        }

        if (ImGui::BeginMenu("Target FPS"))
        {
            const int options[] = {30, 60, 120, 0};
            for (int fps : options)
            {
                char label[16];
                if (fps == 0)
                    std::snprintf(label, sizeof(label), "Uncapped");
                else
                    std::snprintf(label, sizeof(label), "%d", fps);

                const bool selected = target_fps_ == fps;
                if (ImGui::MenuItem(label, nullptr, selected))
                {
                    target_fps_ = fps;
                    SetTargetFPS(target_fps_);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();
        if (ImGui::BeginMenu("Mouse"))
        {
            if (ImGui::MenuItem("Open PhysX (RMB orbit)", nullptr, editor_.mouse == MousePreset::OpenPhysX))
                editor_.mouse = MousePreset::OpenPhysX;
            if (ImGui::MenuItem("Blender (MMB orbit)", nullptr, editor_.mouse == MousePreset::Blender))
                editor_.mouse = MousePreset::Blender;
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Show Grid", nullptr, &simulation.state().show_grid))
        {
            EditSnapshot before;
            before.state = simulation.state();
            before.state.show_grid = !simulation.state().show_grid;
            before.scene = simulation.editor_scene();
            before.active = simulation.active_id();
            editor_.commit_edit(simulation, before, false);
        }
        if (ImGui::MenuItem("Demo Motion", nullptr, &simulation.state().demo_motion))
        {
            EditSnapshot before;
            before.state = simulation.state();
            before.state.demo_motion = !simulation.state().demo_motion;
            before.scene = simulation.editor_scene();
            before.active = simulation.active_id();
            editor_.commit_edit(simulation, before, false);
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Quit"))
        quit_requested_ = true;

    ImGui::EndMainMenuBar();
}

void Workspace::draw_dockspace()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(kDockspaceWindow, nullptr, host_flags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspace_id = ImGui::GetID(kDockspaceId);
    if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr || reset_layout_)
    {
        apply_default_layout(dockspace_id);
        reset_layout_ = false;
    }

    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void Workspace::apply_default_layout(ImGuiID dockspace_id)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

    ImGuiID viewport_id = dockspace_id;
    ImGuiID properties_id = 0;
    ImGuiID animation_id = 0;
    ImGuiID tools_id = 0;

    ImGui::DockBuilderSplitNode(viewport_id, ImGuiDir_Right, 0.28f, &properties_id, &viewport_id);
    ImGui::DockBuilderSplitNode(viewport_id, ImGuiDir_Down, 0.28f, &animation_id, &viewport_id);
    ImGui::DockBuilderSplitNode(properties_id, ImGuiDir_Down, 0.40f, &tools_id, &properties_id);

    ImGui::DockBuilderDockWindow(kViewportTitle, viewport_id);
    ImGui::DockBuilderDockWindow(kPropertiesTitle, properties_id);
    ImGui::DockBuilderDockWindow(kAnimationTitle, animation_id);
    ImGui::DockBuilderDockWindow(kToolsTitle, tools_id);
    ImGui::DockBuilderFinish(dockspace_id);
}

void Workspace::draw_viewport(ISimulation& simulation, IRenderer& renderer)
{
    if (!show_viewport_)
        return;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    const bool visible = ImGui::Begin(
        kViewportTitle,
        &show_viewport_,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);

    if (visible)
    {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        renderer.set_viewport_size(static_cast<int>(size.x), static_cast<int>(size.y));

        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        const ImVec2 overlay = ImGui::GetCursorScreenPos();
        const ImVec2 mouse = ImGui::GetMousePos();
        ViewportSample sample;
        sample.hovered = hovered;
        sample.dx = GetMouseDelta().x;
        sample.dy = GetMouseDelta().y;
        sample.wheel = GetMouseWheelMove();
        sample.width = std::max(size.x, 1.0f);
        sample.height = std::max(size.y, 1.0f);
        sample.aspect = sample.width / sample.height;
        sample.local_x = mouse.x - overlay.x;
        sample.local_y = mouse.y - overlay.y;
        sample.ndc_x = (sample.local_x / sample.width) * 2.0f - 1.0f;
        sample.ndc_y = 1.0f - (sample.local_y / sample.height) * 2.0f;

        const bool text_input = ImGui::GetIO().WantTextInput;
        const bool widget_active = ImGui::GetActiveID() != 0;
        editor_.handle_viewport(simulation, renderer, sample, text_input, widget_active);
        simulation.scene().FlushDestroyed();
        renderer.tick_view(GetFrameTime());
        view_ticked_ = true;
        renderer.render(simulation);

        if (size.x > 1.0f && size.y > 1.0f)
            renderer.draw_viewport_image();

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        char fps[32];
        std::snprintf(fps, sizeof(fps), "%d FPS", GetFPS());
        draw_list->AddText(ImVec2(overlay.x + 10.0f, overlay.y + 8.0f), IM_COL32(255, 255, 255, 210), fps);
        if (autosave_left_ > 0.0f)
        {
            draw_list->AddText(ImVec2(overlay.x + 80.0f, overlay.y + 8.0f), IM_COL32(180, 220, 160, 230), "Autosaved");
            autosave_left_ -= GetFrameTime();
        }
        draw_list->AddText(
            ImVec2(overlay.x + 10.0f, overlay.y + 24.0f),
            IM_COL32(200, 200, 200, 160),
            editor_.nav_help());

        char status[160];
        editor_.status_line(status, static_cast<int>(sizeof(status)));
        if (status[0] != '\0')
        {
            draw_list->AddText(
                ImVec2(overlay.x + 10.0f, overlay.y + 42.0f),
                IM_COL32(232, 158, 62, 230),
                status);
        }

        const char* axis = view_axis_name(renderer.view().view);
        const char* projection =
            renderer.view().projection == ViewProjection::Orthographic ? "Ortho" : "Persp";
        char view_label[64];
        std::snprintf(view_label, sizeof(view_label), "Object   %s   %s", axis, projection);
        draw_list->AddText(
            ImVec2(overlay.x + 10.0f, overlay.y + size.y - 22.0f),
            IM_COL32(200, 200, 200, 180),
            view_label);

        if (editor_.box_visible())
        {
            float x0 = 0.0f;
            float y0 = 0.0f;
            float x1 = 0.0f;
            float y1 = 0.0f;
            editor_.box_rect(x0, y0, x1, y1);
            const ImVec2 a(overlay.x + std::min(x0, x1), overlay.y + std::min(y0, y1));
            const ImVec2 b(overlay.x + std::max(x0, x1), overlay.y + std::max(y0, y1));
            draw_list->AddRectFilled(a, b, IM_COL32(232, 158, 62, 40));
            draw_list->AddRect(a, b, IM_COL32(232, 158, 62, 230));
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

void Workspace::draw_properties(ISimulation& simulation, IRenderer& renderer)
{
    if (!show_properties_)
        return;

    if (!ImGui::Begin(kPropertiesTitle, &show_properties_, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    SimulationState& state = simulation.state();
    View3D& view = renderer.view();
    Entity entity = simulation.active_entity();
    TransformComponent* transform = entity && entity.Has<TransformComponent>() ? &entity.Get<TransformComponent>() : nullptr;
    PrimitiveBoxComponent* box = entity && entity.Has<PrimitiveBoxComponent>() ? &entity.Get<PrimitiveBoxComponent>() : nullptr;
    EditorStateComponent* editor_state = entity && entity.Has<EditorStateComponent>() ? &entity.Get<EditorStateComponent>() : nullptr;
    bool selected = EntitySelected(entity);

    auto arm_edit = [&]() {
        if (ImGui::IsItemActivated())
            edit_before_ = editor_.frame_snapshot();
        if (ImGui::IsItemDeactivatedAfterEdit())
            editor_.commit_edit(simulation, edit_before_, false);
    };

    if (entity && ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (entity.Has<TagComponent>())
            ImGui::TextUnformatted(entity.Get<TagComponent>().name.c_str());
        if (editor_state != nullptr)
        {
            ImGui::Checkbox("Visible", &editor_state->visible);
            arm_edit();
        }
        if (ImGui::Checkbox("Selected", &selected))
            SetEntitySelected(entity, selected);
        if (transform != nullptr)
        {
            ImGui::DragFloat3("Position", &transform->position.x, 0.05f);
            arm_edit();
            if (!euler_active_)
            {
                const Vec3 euler = quat_to_euler_xyz(transform->rotation);
                euler_cache_ = {rad_to_deg(euler.x), rad_to_deg(euler.y), rad_to_deg(euler.z)};
            }
            if (ImGui::DragFloat3("Rotation", &euler_cache_.x, 1.0f))
                transform->rotation = quat_from_euler_xyz(
                    {deg_to_rad(euler_cache_.x), deg_to_rad(euler_cache_.y), deg_to_rad(euler_cache_.z)});
            euler_active_ = ImGui::IsItemActive();
            arm_edit();
        }
        if (box != nullptr)
        {
            ImGui::DragFloat3("Size", &box->size.x, 0.05f, 0.05f, 20.0f);
            arm_edit();
            ImGui::ColorEdit3("Color", box->color.data());
            arm_edit();
        }
    }

    if (ImGui::CollapsingHeader("World", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::ColorEdit3("Clear Color", state.clear_color.data());
        arm_edit();
        ImGui::Checkbox("Show Grid", &state.show_grid);
        arm_edit();
        ImGui::BeginDisabled(!state.show_grid);
        ImGui::SliderInt("Grid Slices", &state.grid_slices, 2, 64);
        arm_edit();
        ImGui::DragFloat("Grid Spacing", &state.grid_spacing, 0.05f, 0.1f, 10.0f);
        arm_edit();
        ImGui::EndDisabled();
        ImGui::Checkbox("Demo Motion", &state.demo_motion);
        arm_edit();
    }

    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const Vec3 eye = view_eye(view);
        const Vec3 pivot = view_pivot(view);
        float eye_values[3] = {eye.x, eye.y, eye.z};
        float pivot_values[3] = {pivot.x, pivot.y, pivot.z};
        ImGui::SliderFloat("FOV", &view.fovy_deg, 20.0f, 90.0f, "%.0f deg");
        bool orthographic = view.projection == ViewProjection::Orthographic;
        if (ImGui::Checkbox("Orthographic", &orthographic))
            renderer.toggle_projection();
        ImGui::BeginDisabled();
        ImGui::DragFloat3("Eye", eye_values);
        ImGui::DragFloat3("Pivot", pivot_values);
        ImGui::EndDisabled();
        ImGui::TextUnformatted(view_axis_name(view.view));
        if (ImGui::Button("Reset Camera", ImVec2(-1.0f, 0.0f)))
            renderer.reset_view();
    }

    if (ImGui::CollapsingHeader("Session"))
    {
        ImGui::Text("raylib  %s", RAYLIB_VERSION);
        ImGui::Text("Dear ImGui  %s", IMGUI_VERSION);
        ImGui::Text("FPS  %d", GetFPS());
        ImGui::Text("Time  %.2f / %.2f s", state.time, state.duration);
    }

    ImGui::End();
}

void Workspace::draw_animation_player(ISimulation& simulation)
{
    if (!show_animation_)
        return;

    if (!ImGui::Begin(kAnimationTitle, &show_animation_, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    SimulationState& state = simulation.state();
    auto arm_edit = [&]() {
        if (ImGui::IsItemActivated())
            edit_before_ = editor_.frame_snapshot();
        if (ImGui::IsItemDeactivatedAfterEdit())
            editor_.commit_edit(simulation, edit_before_, false);
    };

    if (ImGui::Button(ICON_FA_BACKWARD_STEP "##start"))
        simulation.seek(0.0f);
    ImGui::SameLine();

    if (state.playing)
    {
        if (ImGui::Button(ICON_FA_PAUSE " Pause"))
            simulation.pause();
    }
    else
    {
        if (ImGui::Button(ICON_FA_PLAY " Play"))
            simulation.play();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_STOP " Stop"))
    {
        simulation.stop();
        simulation.seek(0.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FORWARD_STEP "##end"))
        simulation.seek(state.duration);

    ImGui::SameLine();
    ImGui::Checkbox("Loop", &state.loop);
    arm_edit();

    ImGui::SetNextItemWidth(-1.0f);
    float time = state.time;
    if (ImGui::SliderFloat("##timeline", &time, 0.0f, std::max(state.duration, 0.001f), "Time %.2f s"))
        simulation.seek(time);

    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Duration", &state.duration, 0.05f, 0.1f, 120.0f, "%.2f s");
    arm_edit();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Speed", &state.playback_speed, 0.05f, 0.1f, 4.0f, "%.2fx");
    arm_edit();

    ImGui::End();
}

void Workspace::draw_tools(ISimulation& simulation, IRenderer& renderer)
{
    if (!show_tools_)
        return;

    if (!ImGui::Begin(kToolsTitle, &show_tools_, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    SimulationState& state = simulation.state();

    ImGui::TextUnformatted("Object Mode");
    ImGui::TextUnformatted("Active Tool");
    if (ImGui::RadioButton(ICON_FA_ARROW_POINTER "  Select", editor_.tool == Tool::Select))
        editor_.tool = Tool::Select;
    if (ImGui::RadioButton(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT "  Move", editor_.tool == Tool::Move))
        editor_.tool = Tool::Move;
    if (ImGui::RadioButton(ICON_FA_ROTATE "  Rotate", editor_.tool == Tool::Rotate))
        editor_.tool = Tool::Rotate;
    if (ImGui::RadioButton(ICON_FA_EXPAND "  Scale", editor_.tool == Tool::Scale))
        editor_.tool = Tool::Scale;
    ImGui::TextWrapped("G move, R rotate, S scale. X Y Z lock an axis. Shift is precise, Ctrl snaps. Enter confirms, Esc cancels.");

    ImGui::Separator();
    ImGui::TextUnformatted("Display");
    ImGui::Checkbox("Show Grid", &state.show_grid);
    ImGui::Checkbox("Demo Motion", &state.demo_motion);

    ImGui::Separator();
    if (ImGui::Button("Reset Simulation", ImVec2(-1.0f, 0.0f)))
        reset_simulation(editor_, simulation);
    if (ImGui::Button("Reset Camera", ImVec2(-1.0f, 0.0f)))
        renderer.reset_view();

    ImGui::Separator();
    ImGui::TextWrapped("Drag a panel tab to undock it. Drop it on a dock node or the window edge to redock. Window > Reset Layout restores the default split.");

    ImGui::End();
}

} // namespace openphysx
