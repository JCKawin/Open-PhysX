#include "ui/Editor.h"

#include "ecs/object_ops.hpp"
#include "ecs/pick.hpp"
#include "ecs/pose.hpp"
#include "ecs/transform_ops.hpp"
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
constexpr float kMinScale = 0.05f;

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
    DeselectAll,
    Hide,
    Show,
    Delete,
    Duplicate,
    Add,
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

// Keys are the contract for the object actions: A and Alt+A select, Shift+A adds, Shift+D
// duplicates, X and Delete remove, H hides, Alt+H shows every hidden object.
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
    {KEY_A, kModAlt, ViewAction::DeselectAll, 0},
    {KEY_A, kModShift, ViewAction::Add, 0},
    {KEY_D, kModShift, ViewAction::Duplicate, 0},
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

Vec3 axis_direction(int axis, bool local, Quat frame)
{
    Vec3 direction{0.0f, 0.0f, 0.0f};
    if (axis == 0)
        direction = {1.0f, 0.0f, 0.0f};
    else if (axis == 1)
        direction = {0.0f, 1.0f, 0.0f};
    else
        direction = {0.0f, 0.0f, 1.0f};
    if (local)
        direction = quat_rotate(frame, direction);
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

// Frames the given objects. The sphere holds every object's box, so the view fits them all.
void frame_objects(ISimulation& simulation, IRenderer& renderer, const std::vector<UUID>& ids, float margin, float aspect)
{
    if (ids.empty())
        return;

    std::vector<RigidBody> bodies;
    Vec3 center{};
    for (const UUID id : ids)
    {
        bodies.push_back(simulation.visual_body_of(id));
        center = vec_add(center, bodies.back().position);
    }
    center = vec_scale(center, 1.0f / static_cast<float>(bodies.size()));

    float radius = 0.0f;
    for (const RigidBody& body : bodies)
        radius = std::max(radius, vec_length(vec_sub(body.position, center)) + BodyRadius(body));
    renderer.frame_bounds(center, radius * margin, aspect, true);
}

std::vector<UUID> selected_drawable(const Scene& scene)
{
    std::vector<UUID> ids;
    for (const UUID id : DrawableObjects(scene))
    {
        if (EntitySelected(scene.FindByUUID(id)))
            ids.push_back(id);
    }
    return ids;
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
    const bool transform = modal_ == Modal::Transform;
    const bool had_base = transform && has_base_;
    if (transform)
    {
        for (const Root& root : roots_)
        {
            if (Entity entity = simulation.scene().FindByUUID(root.id))
                entity.Get<TransformComponent>() = root.local;
        }
    }

    const EditSnapshot base = base_;
    modal_ = Modal::None;
    numeric_.clear();
    axis_ = -1;
    local_axis_ = false;
    box_dragging_ = false;
    roots_.clear();
    has_base_ = false;

    if (had_base)
        commit_edit(simulation, base, false);
}

void Editor::confirm_transform(ISimulation& simulation)
{
    // The live scene holds the moved poses. The snapshot taken before the move holds the
    // original local poses, so undo returns the objects exactly to where they were.
    EditSnapshot before = has_base_ ? base_ : capture(simulation);
    if (!has_base_)
    {
        for (const Root& root : roots_)
        {
            if (Entity entity = before.scene.FindByUUID(root.id))
                entity.Get<TransformComponent>() = root.local;
        }
    }

    modal_ = Modal::None;
    numeric_.clear();
    axis_ = -1;
    local_axis_ = false;
    roots_.clear();
    has_base_ = false;
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
    const std::vector<UUID> ids = TransformRoots(simulation.scene());
    if (ids.empty())
        return;

    cancel_modal(simulation);
    Scene& scene = simulation.scene();
    std::vector<TransformComponent> worlds;
    for (const UUID id : ids)
    {
        const Entity entity = scene.FindByUUID(id);
        Root root;
        root.id = id;
        root.local = entity.Get<TransformComponent>();
        root.world = scene.GetWorldTransform(entity);
        roots_.push_back(root);
        worlds.push_back(root.world);
    }
    pivot_ = MeanPosition(worlds);

    // Local axes follow the active object. With no active root, the first root sets them.
    axis_frame_ = roots_.front().world.rotation;
    for (const Root& root : roots_)
    {
        if (root.id == simulation.active_id())
            axis_frame_ = root.world.rotation;
    }

    modal_ = Modal::Transform;
    xform_ = xform;
    drag_confirm_ = drag_confirm;
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
    const Mods mods = current_mods();
    const bool precise = mods.shift;
    const bool snap = mods.ctrl;
    const float dx = (sample.local_x - press_x_) * (precise ? 0.1f : 1.0f);
    const float dy = (sample.local_y - press_y_) * (precise ? 0.1f : 1.0f);
    const Vec3 right = view_right(view);
    const Vec3 up = view_up(view);
    const Vec3 forward = view_forward(view);

    float typed = 0.0f;
    const bool has_typed = parse_numeric(numeric_, typed);
    Scene& scene = simulation.scene();

    // Every root gets the same edit in world space. Each one is computed from its own pose
    // at the start of the modal, so a frame never accumulates onto the last frame.
    auto write = [&](const Root& root, const TransformComponent& next) {
        if (Entity entity = scene.FindByUUID(root.id))
            scene.SetWorldTransform(entity, next);
    };

    if (xform_ == Xform::Move)
    {
        Vec3 delta{};
        if (has_typed)
        {
            delta = axis_ >= 0 ? vec_scale(axis_direction(axis_, local_axis_, axis_frame_), typed) : vec_scale(right, typed);
        }
        else
        {
            delta = vec_add(vec_scale(right, dx), vec_scale(up, -dy));
            delta = vec_scale(delta, pixel_scale(view));
            if (axis_ >= 0)
            {
                const Vec3 direction = axis_direction(axis_, local_axis_, axis_frame_);
                delta = vec_scale(direction, vec_dot(delta, direction));
            }
            if (snap)
            {
                const float grid = std::max(simulation.state().grid_spacing, 0.001f);
                if (axis_ >= 0)
                {
                    const Vec3 direction = axis_direction(axis_, local_axis_, axis_frame_);
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
        for (const Root& root : roots_)
            write(root, MoveWorld(root.world, delta));
    }
    else if (xform_ == Xform::Rotate)
    {
        float angle = dx * 0.01f;
        if (!has_typed)
        {
            float ndc_x = 0.0f;
            float ndc_y = 0.0f;
            if (view_project(view, pivot_, sample.aspect, ndc_x, ndc_y))
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

        const Vec3 direction = axis_ >= 0 ? axis_direction(axis_, local_axis_, axis_frame_) : forward;
        const Quat rotation = quat_axis_angle(direction, angle);
        for (const Root& root : roots_)
            write(root, RotateWorld(root.world, pivot_, rotation));
    }
    else
    {
        float factor = std::max(0.01f, 1.0f + dx * 0.01f);
        if (has_typed)
            factor = std::max(0.01f, typed);

        for (const Root& root : roots_)
        {
            Vec3 per_axis{1.0f, 1.0f, 1.0f};
            if (axis_ < 0)
                per_axis = {factor, factor, factor};
            else if (axis_ == 0)
                per_axis.x = factor;
            else if (axis_ == 1)
                per_axis.y = factor;
            else
                per_axis.z = factor;

            TransformComponent next = ScaleWorld(root.world, pivot_, per_axis, kMinScale);
            if (snap && !has_typed)
            {
                next.scale.x = std::max(kMinScale, std::round(next.scale.x / 0.1f) * 0.1f);
                next.scale.y = std::max(kMinScale, std::round(next.scale.y / 0.1f) * 0.1f);
                next.scale.z = std::max(kMinScale, std::round(next.scale.z / 0.1f) * 0.1f);
            }
            write(root, next);
        }
    }
}

void Editor::pick_at_cursor(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    Scene& scene = simulation.scene();
    UUID hit = kNullUuid;
    float nearest = 0.0f;
    for (const UUID id : DrawableObjects(scene))
    {
        ScreenBox box;
        if (!ProjectBox(simulation.visual_body_of(id), renderer.view(), sample.width, sample.height, box) ||
            !BoxContains(box, sample.local_x, sample.local_y))
            continue;
        if (hit == kNullUuid || box.depth < nearest)
        {
            hit = id;
            nearest = box.depth;
        }
    }

    UUID active = simulation.active_id();
    if (current_mods().shift)
    {
        if (hit != kNullUuid)
        {
            const bool now = !EntitySelected(scene.FindByUUID(hit));
            SetSelected(scene, hit, now);
            if (now)
                active = hit;
        }
    }
    else
    {
        DeselectAll(scene);
        if (hit != kNullUuid)
        {
            SetSelected(scene, hit, true);
            active = hit;
        }
        else
        {
            active = kNullUuid;
        }
    }
    simulation.set_active(ResolveActive(scene, active));
}

void Editor::finish_box(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    if (!box_dragging_)
    {
        ViewportSample click = sample;
        click.local_x = press_x_;
        click.local_y = press_y_;
        pick_at_cursor(simulation, renderer, click);
        modal_ = Modal::None;
        return;
    }

    Scene& scene = simulation.scene();
    if (!current_mods().shift)
        DeselectAll(scene);
    for (const UUID id : DrawableObjects(scene))
    {
        ScreenBox box;
        if (ProjectBox(simulation.visual_body_of(id), renderer.view(), sample.width, sample.height, box) &&
            BoxOverlaps(box, press_x_, press_y_, sample.local_x, sample.local_y))
            SetSelected(scene, id, true);
    }
    simulation.set_active(ResolveActive(scene, simulation.active_id()));
    modal_ = Modal::None;
    box_dragging_ = false;
}

void Editor::clear_selected(ISimulation& simulation, Clear what)
{
    const std::vector<UUID> ids = TransformRoots(simulation.scene());
    if (ids.empty())
        return;

    const EditSnapshot before = capture(simulation);
    Scene& scene = simulation.scene();
    for (const UUID id : ids)
    {
        Entity entity = scene.FindByUUID(id);
        TransformComponent world = scene.GetWorldTransform(entity);
        if (what == Clear::Location)
            world.position = {0.0f, 1.0f, 0.0f};
        else if (what == Clear::Rotation)
            world.rotation = {};
        else
            world.scale = {1.0f, 1.0f, 1.0f};
        scene.SetWorldTransform(entity, world);
    }
    commit_edit(simulation, before, false);
}

void Editor::try_invoke(ISimulation& simulation, IRenderer& renderer, const ViewportSample& sample)
{
    const Mods mods = current_mods();
    const MouseBind* mouse_binds = mouse == MousePreset::Blender ? kBlenderMouse : kOpenPhysXMouse;
    const int mouse_count = 3;

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

        Scene& scene = simulation.scene();
        switch (bind.action)
        {
        case ViewAction::FrameAll:
            frame_objects(simulation, renderer, DrawableObjects(scene), 1.45f, sample.aspect);
            break;
        case ViewAction::FrameSelection:
            frame_objects(simulation, renderer, selected_drawable(scene), 1.15f, sample.aspect);
            break;
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
            SelectAllVisible(scene);
            simulation.set_active(ResolveActive(scene, simulation.active_id()));
            break;
        case ViewAction::DeselectAll:
            DeselectAll(scene);
            simulation.set_active(kNullUuid);
            break;
        case ViewAction::Hide:
            if (!SelectedObjects(scene).empty())
            {
                const EditSnapshot before = capture(simulation);
                HideSelected(scene);
                commit_edit(simulation, before, false);
            }
            break;
        case ViewAction::Show:
        {
            const EditSnapshot before = capture(simulation);
            RevealHidden(scene);
            simulation.set_active(ResolveActive(scene, simulation.active_id()));
            commit_edit(simulation, before, false);
            break;
        }
        case ViewAction::Delete:
        {
            const std::vector<UUID> doomed = SelectedObjects(scene);
            if (doomed.empty())
                break;
            const EditSnapshot before = capture(simulation);
            DeleteObjects(scene, doomed);
            simulation.set_active(ResolveActive(scene, kNullUuid));
            commit_edit(simulation, before, false);
            break;
        }
        case ViewAction::Duplicate:
        {
            if (SelectedObjects(scene).empty())
                break;
            // The copy and the move that follows are one undo step, so the snapshot is taken
            // before the copy is made.
            const EditSnapshot before = capture(simulation);
            const std::vector<UUID> copies = DuplicateObjects(scene, SelectedObjects(scene));
            if (!copies.empty())
                simulation.set_active(copies.front());
            begin_transform(simulation, Xform::Move, false, sample);
            if (modal_ == Modal::Transform)
            {
                has_base_ = true;
                base_ = before;
            }
            else
            {
                commit_edit(simulation, before, false);
            }
            break;
        }
        case ViewAction::Add:
        {
            const EditSnapshot before = capture(simulation);
            simulation.set_active(AddBoxObject(scene));
            commit_edit(simulation, before, false);
            break;
        }
        case ViewAction::ClearLocation:
            clear_selected(simulation, Clear::Location);
            break;
        case ViewAction::ClearRotation:
            clear_selected(simulation, Clear::Rotation);
            break;
        case ViewAction::ClearScale:
            clear_selected(simulation, Clear::Scale);
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

void Editor::outliner_click(ISimulation& simulation, UUID id, bool additive)
{
    Scene& scene = simulation.scene();
    Entity entity = scene.FindByUUID(id);
    if (!entity)
        return;

    UUID active = simulation.active_id();
    if (additive)
    {
        const bool now = !EntitySelected(entity);
        SetEntitySelected(entity, now);
        if (now)
            active = id;
    }
    else
    {
        DeselectAll(scene);
        SetEntitySelected(entity, true);
        active = id;
    }
    simulation.set_active(ResolveActive(scene, active));
}

void Editor::set_visible(ISimulation& simulation, UUID id, bool visible)
{
    Entity entity = simulation.scene().FindByUUID(id);
    if (!entity || !entity.Has<EditorStateComponent>() || entity.Get<EditorStateComponent>().visible == visible)
        return;

    const EditSnapshot before = capture(simulation);
    entity.Get<EditorStateComponent>().visible = visible;
    commit_edit(simulation, before, false);
}

void Editor::rename_object(ISimulation& simulation, UUID id, const std::string& name)
{
    Entity entity = simulation.scene().FindByUUID(id);
    if (!entity || !entity.Has<TagComponent>() || name.empty())
        return;

    const EditSnapshot before = capture(simulation);
    simulation.scene().SetName(entity, name);
    commit_edit(simulation, before, false);
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
        return "MMB orbit   Shift+MMB pan   Wheel zoom   G R S transform   A all   Alt+A none   Shift+A add   Shift+D dup   X delete";
    return "RMB orbit   Shift+RMB pan   Wheel zoom   G R S transform   A all   Alt+A none   Shift+A add   Shift+D dup   X delete";
}

} // namespace openphysx
