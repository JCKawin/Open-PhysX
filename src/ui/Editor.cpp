#include "ui/Editor.h"

#include "ecs/pose.hpp"
#include "logic/ISimulation.h"
#include "renderer/IRenderer.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace openphysx {
namespace {

constexpr int kModShift = 1;
constexpr int kModCtrl = 2;
constexpr int kModAlt = 4;
constexpr float kOrbitStep = 15.0f * kPi / 180.0f;

struct Mods
{
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

enum class AppAction
{
    Quit,
    Reset,
    Undo,
    Redo,
};

enum class ViewAction
{
    FrameAll,
    FrameSelection,
    TogglePersp,
    OrbitStep,
    Axis,
    Move,
    Rotate,
    Scale,
    SelectAll,
    Hide,
    Show,
    Delete,
    ClearLocation,
    ClearRotation,
    ClearScale,
    Box,
};

struct AppBind
{
    int key;
    int mods;
    AppAction action;
};

struct KeyBind
{
    int key;
    int mods;
    ViewAction action;
    int prop;
};

struct MouseBind
{
    int button;
    int mods;
    int kind;
};

constexpr int kOrbit = 1;
constexpr int kPan = 2;
constexpr int kZoom = 3;

constexpr AppBind kAppBinds[] = {
    {KEY_Z, kModCtrl | kModShift, AppAction::Redo},
    {KEY_Z, kModCtrl, AppAction::Undo},
};

constexpr KeyBind kViewBinds[] = {
    {KEY_HOME, 0, ViewAction::FrameAll, 0},
    {KEY_KP_DECIMAL, 0, ViewAction::FrameSelection, 0},
    {KEY_KP_5, 0, ViewAction::TogglePersp, 0},
    {KEY_KP_1, kModCtrl, ViewAction::Axis, static_cast<int>(ViewAxis::Back)},
    {KEY_KP_1, 0, ViewAction::Axis, static_cast<int>(ViewAxis::Front)},
    {KEY_KP_3, kModCtrl, ViewAction::Axis, static_cast<int>(ViewAxis::Left)},
    {KEY_KP_3, 0, ViewAction::Axis, static_cast<int>(ViewAxis::Right)},
    {KEY_KP_7, kModCtrl, ViewAction::Axis, static_cast<int>(ViewAxis::Bottom)},
    {KEY_KP_7, 0, ViewAction::Axis, static_cast<int>(ViewAxis::Top)},
    {KEY_KP_4, 0, ViewAction::OrbitStep, 0},
    {KEY_KP_6, 0, ViewAction::OrbitStep, 1},
    {KEY_KP_2, 0, ViewAction::OrbitStep, 2},
    {KEY_KP_8, 0, ViewAction::OrbitStep, 3},
    {KEY_G, kModAlt, ViewAction::ClearLocation, 0},
    {KEY_R, kModAlt, ViewAction::ClearRotation, 0},
    {KEY_S, kModAlt, ViewAction::ClearScale, 0},
    {KEY_G, 0, ViewAction::Move, 0},
    {KEY_R, 0, ViewAction::Rotate, 0},
    {KEY_S, 0, ViewAction::Scale, 0},
    {KEY_A, 0, ViewAction::SelectAll, 0},
    {KEY_B, 0, ViewAction::Box, 0},
    {KEY_H, kModAlt, ViewAction::Show, 0},
    {KEY_H, 0, ViewAction::Hide, 0},
    {KEY_X, 0, ViewAction::Delete, 0},
    {KEY_DELETE, 0, ViewAction::Delete, 0},
};

constexpr MouseBind kOpenPhysXMouse[] = {
    {MOUSE_BUTTON_RIGHT, kModCtrl, kZoom},
    {MOUSE_BUTTON_RIGHT, kModShift, kPan},
    {MOUSE_BUTTON_RIGHT, 0, kOrbit},
};

constexpr MouseBind kBlenderMouse[] = {
    {MOUSE_BUTTON_MIDDLE, kModCtrl, kZoom},
    {MOUSE_BUTTON_MIDDLE, kModShift, kPan},
    {MOUSE_BUTTON_MIDDLE, 0, kOrbit},
};

Mods current_mods()
{
    return {
        IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL),
        IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT),
    };
}

bool mods_match(Mods mods, int mask)
{
    return mods.shift == ((mask & kModShift) != 0) && mods.ctrl == ((mask & kModCtrl) != 0) &&
           mods.alt == ((mask & kModAlt) != 0);
}

EditSnapshot capture(const ISimulation& simulation)
{
    EditSnapshot shot;
    shot.state = simulation.state();
    shot.scene = simulation.editor_scene();
    shot.active = simulation.active_id();
    return shot;
}

bool same_edit(const EditSnapshot& a, const EditSnapshot& b)
{
    SimulationState left = a.state;
    SimulationState right = b.state;
    left.time = right.time = 0.0f;
    left.playing = right.playing = false;
    return left == right && a.scene == b.scene && a.active == b.active;
}

float body_radius(const RigidBody& body)
{
    return 0.5f * std::sqrt(body.size.x * body.size.x + body.size.y * body.size.y + body.size.z * body.size.z);
}

bool body_screen_rect(
    const RigidBody& body, const View3D& view, const ViewportSample& sample, float& min_x, float& min_y, float& max_x, float& max_y)
{
    const Vec3 half = vec_scale(body.size, 0.5f);
    bool any = false;
    min_x = min_y = 1.0e9f;
    max_x = max_y = -1.0e9f;

    for (int i = 0; i < 8; ++i)
    {
        const Vec3 corner{
            (i & 1) ? half.x : -half.x,
            (i & 2) ? half.y : -half.y,
            (i & 4) ? half.z : -half.z,
        };
        const Vec3 world = vec_add(body.position, quat_rotate(body.rotation, corner));
        float ndc_x = 0.0f;
        float ndc_y = 0.0f;
        if (!view_project(view, world, sample.aspect, ndc_x, ndc_y))
            continue;

        const float x = (ndc_x * 0.5f + 0.5f) * sample.width;
        const float y = (1.0f - (ndc_y * 0.5f + 0.5f)) * sample.height;
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
        any = true;
    }
    return any;
}

bool point_in_rect(float x, float y, float min_x, float min_y, float max_x, float max_y)
{
    return x >= min_x && x <= max_x && y >= min_y && y <= max_y;
}

bool ranges_overlap(float a0, float a1, float b0, float b1)
{
    return std::min(a0, a1) <= std::max(b0, b1) && std::max(a0, a1) >= std::min(b0, b1);
}

Vec3 axis_direction(int axis, bool local, const RigidBody& body)
{
    Vec3 direction{0.0f, 0.0f, 0.0f};
    if (axis == 0)
        direction = {1.0f, 0.0f, 0.0f};
    else if (axis == 1)
        direction = {0.0f, 1.0f, 0.0f};
    else
        direction = {0.0f, 0.0f, 1.0f};
    if (local)
        direction = quat_rotate(body.rotation, direction);
    return vec_normalize(direction);
}

float pixel_scale(const View3D& view)
{
    const float span = view.projection == ViewProjection::Perspective ? view.dist : view.ortho_height;
    return span * 0.0015f;
}

bool take_number_key(std::string& numeric)
{
    if (IsKeyPressed(KEY_BACKSPACE))
    {
        if (!numeric.empty())
            numeric.pop_back();
        return true;
    }

    auto push_digit = [&](char digit) {
        if (numeric.size() < 16)
            numeric.push_back(digit);
    };

    for (int key = KEY_ZERO; key <= KEY_NINE; ++key)
    {
        if (IsKeyPressed(key))
        {
            push_digit(static_cast<char>('0' + (key - KEY_ZERO)));
            return true;
        }
    }
    for (int key = KEY_KP_0; key <= KEY_KP_9; ++key)
    {
        if (IsKeyPressed(key))
        {
            push_digit(static_cast<char>('0' + (key - KEY_KP_0)));
            return true;
        }
    }
    if (IsKeyPressed(KEY_PERIOD) || IsKeyPressed(KEY_KP_DECIMAL))
    {
        if (numeric.find('.') == std::string::npos)
            push_digit('.');
        return true;
    }
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
    {
        if (numeric.empty())
            numeric.push_back('-');
        return true;
    }
    return false;
}

bool parse_numeric(const std::string& numeric, float& value)
{
    if (numeric.empty() || numeric == "-" || numeric == "." || numeric == "-.")
        return false;
    char* end = nullptr;
    value = std::strtof(numeric.c_str(), &end);
    return end != numeric.c_str() && end != nullptr && *end == '\0';
}

} // namespace

void Editor::begin_frame(const ISimulation& simulation)
{
    frame_ = capture(simulation);
}

void Editor::commit_edit(ISimulation& simulation, const EditSnapshot& before, bool full)
{
    // Play mode edits the runtime copy. They are thrown away on stop and must not enter undo.
    if (simulation.simulating())
        return;

    const EditSnapshot after = capture(simulation);
    if (full)
    {
        if (before.state == after.state && before.scene == after.scene && before.active == after.active)
            return;
    }
    else if (same_edit(before, after))
    {
        return;
    }

    commands_.Commit(before, after, full);
}

void Editor::cancel_modal(ISimulation& simulation)
{
    if (modal_ == Modal::Transform)
    {
        SetEntityPose(simulation.active_entity(), body_before_);
    }
    modal_ = Modal::None;
    numeric_.clear();
    axis_ = -1;
    local_axis_ = false;
    box_dragging_ = false;
}

void Editor::confirm_transform(ISimulation& simulation)
{
    EditSnapshot before = capture(simulation);
    SetEntityPose(before.scene.FindByUUID(before.active), body_before_);
    modal_ = Modal::None;
    numeric_.clear();
    axis_ = -1;
    local_axis_ = false;
    commit_edit(simulation, before, false);
}

void Editor::undo(ISimulation& simulation)
{
    cancel_modal(simulation);
    commands_.Undo(simulation);
}

void Editor::redo(ISimulation& simulation)
{
    cancel_modal(simulation);
    commands_.Redo(simulation);
}

void Editor::handle_app(ISimulation& simulation, bool text_input, bool item_focused, bool& quit)
{
    if (text_input)
        return;

    const Mods mods = current_mods();
    for (const AppBind& bind : kAppBinds)
    {
        if (!IsKeyPressed(bind.key) || !mods_match(mods, bind.mods))
            continue;

        switch (bind.action)
        {
        case AppAction::Quit:
            quit = true;
            break;
        case AppAction::Reset:
        {
            cancel_modal(simulation);
            const EditSnapshot before = capture(simulation);
            simulation.reset();
            commit_edit(simulation, before, true);
            break;
        }
        case AppAction::Undo:
            undo(simulation);
            break;
        case AppAction::Redo:
            redo(simulation);
            break;
        }
        return;
    }

    if (!item_focused && IsKeyPressed(KEY_SPACE))
    {
        if (simulation.state().playing)
            simulation.pause();
        else
            simulation.play();
    }
}

void Editor::begin_transform(ISimulation& simulation, Xform xform, bool drag_confirm, const ViewportSample& sample)
{
    const Entity entity = simulation.active_entity();
    if (!EntityVisible(entity))
        return;

    cancel_modal(simulation);
    modal_ = Modal::Transform;
    xform_ = xform;
    drag_confirm_ = drag_confirm;
    body_before_ = EntityPose(entity);
    SetEntitySelected(entity, true);
    press_x_ = sample.local_x;
    press_y_ = sample.local_y;
    axis_ = -1;
    local_axis_ = false;
    numeric_.clear();

    if (xform == Xform::Move)
        tool = Tool::Move;
    else if (xform == Xform::Rotate)
        tool = Tool::Rotate;
    else
        tool = Tool::Scale;
}

void Editor::apply_transform(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    const View3D& view = renderer.view();
    const bool precise = current_mods().shift;
    const bool snap = current_mods().ctrl;
    const float dx = (sample.local_x - press_x_) * (precise ? 0.1f : 1.0f);
    const float dy = (sample.local_y - press_y_) * (precise ? 0.1f : 1.0f);
    const Vec3 right = view_right(view);
    const Vec3 up = view_up(view);
    const Vec3 forward = view_forward(view);

    float typed = 0.0f;
    const bool has_typed = parse_numeric(numeric_, typed);
    RigidBody body = body_before_;

    if (xform_ == Xform::Move)
    {
        Vec3 delta{};
        if (has_typed)
        {
            delta = axis_ >= 0 ? vec_scale(axis_direction(axis_, local_axis_, body_before_), typed) : vec_scale(right, typed);
        }
        else
        {
            delta = vec_add(vec_scale(right, dx), vec_scale(up, -dy));
            delta = vec_scale(delta, pixel_scale(view));
            if (axis_ >= 0)
            {
                const Vec3 direction = axis_direction(axis_, local_axis_, body_before_);
                delta = vec_scale(direction, vec_dot(delta, direction));
            }
            if (snap)
            {
                const float grid = std::max(simulation.state().grid_spacing, 0.001f);
                if (axis_ >= 0)
                {
                    const Vec3 direction = axis_direction(axis_, local_axis_, body_before_);
                    const float along = std::round(vec_dot(delta, direction) / grid) * grid;
                    delta = vec_scale(direction, along);
                }
                else
                {
                    delta.x = std::round(delta.x / grid) * grid;
                    delta.y = std::round(delta.y / grid) * grid;
                    delta.z = std::round(delta.z / grid) * grid;
                }
            }
        }
        body.position = vec_add(body_before_.position, delta);
    }
    else if (xform_ == Xform::Rotate)
    {
        float angle = dx * 0.01f;
        if (!has_typed)
        {
            float ndc_x = 0.0f;
            float ndc_y = 0.0f;
            const Vec3 pivot = simulation.visual_body().position;
            if (view_project(view, pivot, sample.aspect, ndc_x, ndc_y))
            {
                const float cx = (ndc_x * 0.5f + 0.5f) * sample.width;
                const float cy = (1.0f - (ndc_y * 0.5f + 0.5f)) * sample.height;
                const float a0 = std::atan2(press_y_ - cy, press_x_ - cx);
                const float a1 = std::atan2(sample.local_y - cy, sample.local_x - cx);
                angle = (a1 - a0) * (precise ? 0.1f : 1.0f);
            }
            if (snap)
                angle = std::round(angle / kOrbitStep) * kOrbitStep;
        }
        else
        {
            angle = deg_to_rad(typed);
        }

        const Vec3 direction = axis_ >= 0 ? axis_direction(axis_, local_axis_, body_before_) : forward;
        body.rotation = quat_normalize(quat_mul(quat_axis_angle(direction, angle), body_before_.rotation));
    }
    else
    {
        float factor = std::max(0.01f, 1.0f + dx * 0.01f);
        if (has_typed)
            factor = std::max(0.01f, typed);

        auto scale_component = [&](float value) {
            float scaled = std::max(0.05f, value * factor);
            if (snap && !has_typed)
                scaled = std::max(0.05f, std::round(scaled / 0.1f) * 0.1f);
            return scaled;
        };

        if (axis_ < 0)
        {
            body.size.x = scale_component(body_before_.size.x);
            body.size.y = scale_component(body_before_.size.y);
            body.size.z = scale_component(body_before_.size.z);
        }
        else if (axis_ == 0)
            body.size.x = scale_component(body_before_.size.x);
        else if (axis_ == 1)
            body.size.y = scale_component(body_before_.size.y);
        else
            body.size.z = scale_component(body_before_.size.z);
    }

    SetEntityPose(simulation.active_entity(), body);
}

void Editor::click_select(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    const Entity entity = simulation.active_entity();
    if (!entity)
        return;
    if (!EntityVisible(entity))
    {
        SetEntitySelected(entity, false);
        return;
    }

    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    const bool projected = body_screen_rect(simulation.visual_body(), renderer.view(), sample, min_x, min_y, max_x, max_y);
    SetEntitySelected(entity, projected && point_in_rect(sample.local_x, sample.local_y, min_x, min_y, max_x, max_y));
}

void Editor::finish_box(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    if (!box_dragging_)
    {
        ViewportSample click = sample;
        click.local_x = press_x_;
        click.local_y = press_y_;
        click_select(simulation, renderer, click);
        modal_ = Modal::None;
        return;
    }

    const Entity entity = simulation.active_entity();
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    const bool projected = EntityVisible(entity) &&
                           body_screen_rect(simulation.visual_body(), renderer.view(), sample, min_x, min_y, max_x, max_y);
    SetEntitySelected(
        entity,
        projected && ranges_overlap(press_x_, sample.local_x, min_x, max_x) &&
            ranges_overlap(press_y_, sample.local_y, min_y, max_y));
    modal_ = Modal::None;
    box_dragging_ = false;
}

void Editor::try_invoke(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    const Mods mods = current_mods();
    const MouseBind* mouse_binds = mouse == MousePreset::Blender ? kBlenderMouse : kOpenPhysXMouse;
    const int mouse_count = mouse == MousePreset::Blender ? 3 : 3;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        if (tool == Tool::Select)
        {
            modal_ = Modal::Box;
            box_dragging_ = false;
            press_x_ = sample.local_x;
            press_y_ = sample.local_y;
            return;
        }
        const Xform xform = tool == Tool::Rotate ? Xform::Rotate : tool == Tool::Scale ? Xform::Scale : Xform::Move;
        begin_transform(simulation, xform, true, sample);
        return;
    }

    for (int i = 0; i < mouse_count; ++i)
    {
        const MouseBind& bind = mouse_binds[i];
        if (!IsMouseButtonPressed(bind.button) || !mods_match(mods, bind.mods))
            continue;
        if (bind.kind == kOrbit)
            modal_ = Modal::Orbit;
        else if (bind.kind == kPan)
            modal_ = Modal::Pan;
        else
            modal_ = Modal::Zoom;
        return;
    }

    if (sample.wheel != 0.0f)
    {
        renderer.zoom_at(sample.wheel, sample.ndc_x, sample.ndc_y, sample.aspect);
        return;
    }

    for (const KeyBind& bind : kViewBinds)
    {
        if (!IsKeyPressed(bind.key) || !mods_match(mods, bind.mods))
            continue;

        switch (bind.action)
        {
        case ViewAction::FrameAll:
        case ViewAction::FrameSelection:
        {
            const Entity entity = simulation.active_entity();
            if (!entity)
                break;
            const RigidBody body = EntityPose(entity);
            const float radius = body_radius(body) * (bind.action == ViewAction::FrameAll ? 1.45f : 1.15f);
            renderer.frame_bounds(body.position, radius, sample.aspect, true);
            break;
        }
        case ViewAction::TogglePersp:
            renderer.toggle_projection();
            break;
        case ViewAction::OrbitStep:
            if (bind.prop == 0)
                renderer.orbit_step(-kOrbitStep, 0.0f, true);
            else if (bind.prop == 1)
                renderer.orbit_step(kOrbitStep, 0.0f, true);
            else if (bind.prop == 2)
                renderer.orbit_step(0.0f, -kOrbitStep, true);
            else
                renderer.orbit_step(0.0f, kOrbitStep, true);
            break;
        case ViewAction::Axis:
            renderer.set_axis(static_cast<ViewAxis>(bind.prop), true);
            break;
        case ViewAction::Move:
            begin_transform(simulation, Xform::Move, false, sample);
            break;
        case ViewAction::Rotate:
            begin_transform(simulation, Xform::Rotate, false, sample);
            break;
        case ViewAction::Scale:
            begin_transform(simulation, Xform::Scale, false, sample);
            break;
        case ViewAction::SelectAll:
            SetEntitySelected(simulation.active_entity(), true);
            break;
        case ViewAction::Hide:
        case ViewAction::Delete:
            if (Entity entity = simulation.active_entity(); EntityVisible(entity))
            {
                const EditSnapshot before = capture(simulation);
                entity.Get<EditorStateComponent>().visible = false;
                commit_edit(simulation, before, false);
            }
            break;
        case ViewAction::Show:
            if (Entity entity = simulation.active_entity(); entity && !EntityVisible(entity))
            {
                const EditSnapshot before = capture(simulation);
                entity.Get<EditorStateComponent>().visible = true;
                commit_edit(simulation, before, false);
            }
            break;
        case ViewAction::ClearLocation:
            if (Entity entity = simulation.active_entity(); entity && entity.Has<TransformComponent>())
            {
                const EditSnapshot before = capture(simulation);
                entity.Get<TransformComponent>().position = {0.0f, 1.0f, 0.0f};
                commit_edit(simulation, before, false);
            }
            break;
        case ViewAction::ClearRotation:
            if (Entity entity = simulation.active_entity(); entity && entity.Has<TransformComponent>())
            {
                const EditSnapshot before = capture(simulation);
                entity.Get<TransformComponent>().rotation = {};
                commit_edit(simulation, before, false);
            }
            break;
        case ViewAction::ClearScale:
            if (Entity entity = simulation.active_entity(); entity && entity.Has<PrimitiveBoxComponent>())
            {
                const EditSnapshot before = capture(simulation);
                entity.Get<PrimitiveBoxComponent>().size = {2.0f, 2.0f, 2.0f};
                commit_edit(simulation, before, false);
            }
            break;
        case ViewAction::Box:
            modal_ = Modal::Box;
            box_dragging_ = true;
            press_x_ = sample.local_x;
            press_y_ = sample.local_y;
            tool = Tool::Select;
            break;
        }
        return;
    }
}

void Editor::handle_viewport(
    ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample, bool text_input, bool widget_active)
{
    cursor_x_ = sample.local_x;
    cursor_y_ = sample.local_y;

    if (modal_ == Modal::None)
    {
        if (sample.hovered && !text_input && !widget_active)
            try_invoke(simulation, renderer, sample);
        return;
    }

    if (modal_ == Modal::Orbit || modal_ == Modal::Pan || modal_ == Modal::Zoom)
    {
        const int button = mouse == MousePreset::Blender ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_RIGHT;
        if (!IsMouseButtonDown(button))
        {
            modal_ = Modal::None;
            return;
        }
        if (modal_ == Modal::Orbit)
            renderer.orbit(sample.dx, sample.dy);
        else if (modal_ == Modal::Pan)
            renderer.pan(sample.dx, sample.dy);
        else
            renderer.zoom_at(-sample.dy * 0.05f, sample.ndc_x, sample.ndc_y, sample.aspect);

        if (sample.wheel != 0.0f && modal_ != Modal::Zoom)
            renderer.zoom_at(sample.wheel, sample.ndc_x, sample.ndc_y, sample.aspect);
        return;
    }

    if (modal_ == Modal::Box)
    {
        if (!box_dragging_ && (std::fabs(sample.local_x - press_x_) > 4.0f || std::fabs(sample.local_y - press_y_) > 4.0f))
            box_dragging_ = true;
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            finish_box(simulation, renderer, sample);
        return;
    }

    const Mods mods = current_mods();
    if (!mods.alt && IsKeyPressed(KEY_X))
    {
        if (axis_ == 0 && !local_axis_)
            local_axis_ = true;
        else if (axis_ == 0 && local_axis_)
        {
            axis_ = -1;
            local_axis_ = false;
        }
        else
        {
            axis_ = 0;
            local_axis_ = false;
        }
    }
    else if (!mods.alt && IsKeyPressed(KEY_Y))
    {
        if (axis_ == 1 && !local_axis_)
            local_axis_ = true;
        else if (axis_ == 1 && local_axis_)
        {
            axis_ = -1;
            local_axis_ = false;
        }
        else
        {
            axis_ = 1;
            local_axis_ = false;
        }
    }
    else if (!mods.alt && IsKeyPressed(KEY_Z) && !mods.ctrl)
    {
        if (axis_ == 2 && !local_axis_)
            local_axis_ = true;
        else if (axis_ == 2 && local_axis_)
        {
            axis_ = -1;
            local_axis_ = false;
        }
        else
        {
            axis_ = 2;
            local_axis_ = false;
        }
    }
    else
    {
        take_number_key(numeric_);
    }

    apply_transform(simulation, renderer, sample);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) || IsKeyPressed(KEY_ESCAPE))
    {
        if (IsKeyPressed(KEY_ESCAPE))
            cancel_modal(simulation);
        else
            confirm_transform(simulation);
        return;
    }

    if (drag_confirm_)
    {
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsKeyPressed(KEY_ESCAPE))
        {
            if (IsKeyPressed(KEY_ESCAPE))
                cancel_modal(simulation);
            else
                confirm_transform(simulation);
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        confirm_transform(simulation);
    else if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE))
        cancel_modal(simulation);
}

bool Editor::box_visible() const
{
    return modal_ == Modal::Box && box_dragging_;
}

void Editor::box_rect(float& x0, float& y0, float& x1, float& y1) const
{
    x0 = press_x_;
    y0 = press_y_;
    x1 = cursor_x_;
    y1 = cursor_y_;
}

void Editor::status_line(char* buffer, int size) const
{
    if (buffer == nullptr || size <= 0)
        return;
    buffer[0] = '\0';
    if (modal_ != Modal::Transform)
        return;

    const char* name = xform_ == Xform::Move ? "Move" : xform_ == Xform::Rotate ? "Rotate" : "Scale";
    const char* axis = "";
    if (axis_ == 0)
        axis = local_axis_ ? "  local X" : "  X";
    else if (axis_ == 1)
        axis = local_axis_ ? "  local Y" : "  Y";
    else if (axis_ == 2)
        axis = local_axis_ ? "  local Z" : "  Z";

    std::snprintf(
        buffer,
        static_cast<size_t>(size),
        "%s%s%s%s   Enter confirm   Esc cancel",
        name,
        axis,
        numeric_.empty() ? "" : "  ",
        numeric_.c_str());
}

const char* Editor::nav_help() const
{
    if (mouse == MousePreset::Blender)
        return "MMB orbit   Shift+MMB pan   Wheel zoom   G R S transform   Numpad views";
    return "RMB orbit   Shift+RMB pan   Wheel zoom   G R S transform   Numpad views";
}

} // namespace openphysx
