#pragma once

#include "core/View.h"
#include "editor/command.hpp"
#include "ecs/scene.hpp"

#include <string>
#include <vector>

namespace openphysx {

class IRenderer;
class ISimulation;

enum class Tool
{
    Select,
    Move,
    Rotate,
    Scale,
};

enum class MousePreset
{
    OpenPhysX,
    Blender,
};

struct ViewportSample
{
    bool hovered = false;
    float dx = 0.0f;
    float dy = 0.0f;
    float ndc_x = 0.0f;
    float ndc_y = 0.0f;
    float aspect = 1.0f;
    float wheel = 0.0f;
    float local_x = 0.0f;
    float local_y = 0.0f;
    float width = 1.0f;
    float height = 1.0f;
};

// Keymap, modal operators, and the editor command stack. UI code only.
// Selection changes are not commands. Edits to the scene, visibility, and names are.
class Editor
{
public:
    void begin_frame(const ISimulation& simulation);
    const EditSnapshot& frame_snapshot() const { return frame_; }

    void handle_app(ISimulation& simulation, bool text_input, bool item_focused, bool& quit);
    void handle_viewport(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample, bool text_input, bool widget_active);

    void commit_edit(ISimulation& simulation, const EditSnapshot& before, bool full);
    void undo(ISimulation& simulation);
    void redo(ISimulation& simulation);
    bool modal_active() const { return modal_ != Modal::None; }
    bool can_undo() const { return commands_.CanUndo(); }
    bool can_redo() const { return commands_.CanRedo(); }
    CommandStack& commands() { return commands_; }
    const CommandStack& commands() const { return commands_; }

    // Outliner actions. A click selects. Shift-click adds or removes from the selection.
    void outliner_click(ISimulation& simulation, UUID id, bool additive);
    void set_visible(ISimulation& simulation, UUID id, bool visible);
    void rename_object(ISimulation& simulation, UUID id, const std::string& name);

    Tool tool = Tool::Select;
    MousePreset mouse = MousePreset::OpenPhysX;

    bool box_visible() const;
    void box_rect(float& x0, float& y0, float& x1, float& y1) const;
    void status_line(char* buffer, int size) const;
    const char* nav_help() const;

private:
    enum class Modal
    {
        None,
        Orbit,
        Pan,
        Zoom,
        Transform,
        Box,
    };

    enum class Xform
    {
        Move,
        Rotate,
        Scale,
    };

    enum class Clear
    {
        Location,
        Rotation,
        Scale,
    };

    // One selected object the transform moves. Objects under a selected parent follow it.
    struct Root
    {
        UUID id = kNullUuid;
        TransformComponent local{};
        TransformComponent world{};
    };

    void cancel_modal(ISimulation& simulation);
    void confirm_transform(ISimulation& simulation);
    void begin_transform(ISimulation& simulation, Xform xform, bool drag_confirm, const ViewportSample& sample);
    void apply_transform(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);
    void finish_box(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);
    void pick_at_cursor(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);
    void clear_selected(ISimulation& simulation, Clear what);
    void try_invoke(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);

    EditSnapshot frame_{};
    CommandStack commands_;

    Modal modal_ = Modal::None;
    Xform xform_ = Xform::Move;
    bool drag_confirm_ = false;
    bool box_dragging_ = false;
    int axis_ = -1;
    bool local_axis_ = false;
    std::vector<Root> roots_;
    Vec3 pivot_{};
    Quat axis_frame_{};
    // A duplicate starts a move. Undo treats the copy and the move as one step.
    bool has_base_ = false;
    EditSnapshot base_{};
    float press_x_ = 0.0f;
    float press_y_ = 0.0f;
    float cursor_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    std::string numeric_;
};

} // namespace openphysx
