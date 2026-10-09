#pragma once

#include "core/View.h"
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

// Transport block plus the scene. A pose edit has to keep the active object's transform,
// because that transform no longer lives on SimulationState.
struct EditSnapshot
{
    SimulationState state{};
    Scene scene{};
    UUID active = kNullUuid;
};

// Keymap, modal operators, and the editor undo stack. UI code only.
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
    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }

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

    struct HistoryItem
    {
        EditSnapshot shot{};
        bool full = false;
    };

    void restore(ISimulation& simulation, const EditSnapshot& shot, bool full);
    void cancel_modal(ISimulation& simulation);
    void confirm_transform(ISimulation& simulation);
    void begin_transform(ISimulation& simulation, Xform xform, bool drag_confirm, const ViewportSample& sample);
    void apply_transform(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);
    void finish_box(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);
    void click_select(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);
    void try_invoke(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample);

    EditSnapshot frame_{};
    std::vector<HistoryItem> undo_;
    std::vector<HistoryItem> redo_;

    Modal modal_ = Modal::None;
    Xform xform_ = Xform::Move;
    bool drag_confirm_ = false;
    bool box_dragging_ = false;
    int axis_ = -1;
    bool local_axis_ = false;
    RigidBody body_before_{};
    float press_x_ = 0.0f;
    float press_y_ = 0.0f;
    float cursor_x_ = 0.0f;
    float cursor_y_ = 0.0f;
    std::string numeric_;
};

} // namespace openphysx
