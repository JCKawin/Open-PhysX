#pragma once

#include "imgui.h"

namespace openphysx {

class ISimulation;
class IRenderer;

class Workspace
{
public:
    enum class Tool
    {
        Select,
        Move,
        Rotate,
        Scale,
    };

    void init();
    void draw(ISimulation& simulation, IRenderer& renderer);

    bool quit_requested() const { return quit_requested_; }
    void request_quit() { quit_requested_ = true; }

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
    Tool tool_ = Tool::Select;
};

} // namespace openphysx
