#pragma once

#include "ui/Editor.h"
#include "ui/file_menu.hpp"

#include "imgui.h"

namespace openphysx {

class ISimulation;
class IRenderer;

class Workspace
{
public:
    void init();
    void shutdown();
    void draw(ISimulation& simulation, IRenderer& renderer);

    bool quit_requested() const { return quit_requested_; }
    void request_quit(ISimulation& simulation, IRenderer& renderer);

private:
    void apply_theme();
    void draw_menu_bar(ISimulation& simulation, IRenderer& renderer);
    void draw_dockspace();
    void apply_default_layout(ImGuiID dockspace_id);
    void draw_viewport(ISimulation& simulation, IRenderer& renderer);
    void draw_properties(ISimulation& simulation, IRenderer& renderer);
    void draw_animation_player(ISimulation& simulation);
    void draw_tools(ISimulation& simulation, IRenderer& renderer);

    bool quit_requested_ = false;
    bool reset_layout_ = false;
    bool show_viewport_ = true;
    bool show_properties_ = true;
    bool show_animation_ = true;
    bool show_tools_ = true;
    bool show_demo_ = false;
    bool show_metrics_ = false;
    bool vsync_ = true;
    int target_fps_ = 60;
    bool view_ticked_ = false;
    bool euler_active_ = false;
    Vec3 euler_cache_{};
    EditSnapshot edit_before_{};
    Editor editor_{};
    FileSession files_{};
};

} // namespace openphysx
