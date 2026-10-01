#include "renderer/Renderer.h"

#include "logic/ISimulation.h"

#include "rlImGui.h"
#include "rlgl.h"

#include <algorithm>
#include <cstdio>

namespace openphysx {
namespace {

Color to_color(const Rgb& rgb)
{
    return Color{
        static_cast<unsigned char>(std::clamp(rgb.r, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(rgb.g, 0.0f, 1.0f) * 255.0f),
        static_cast<unsigned char>(std::clamp(rgb.b, 0.0f, 1.0f) * 255.0f),
        255,
    };
}

Vector3 to_vec3(const Vec3& v)
{
    return Vector3{v.x, v.y, v.z};
}

void draw_body(const RigidBody& body, bool selected)
{
    Vec3 x_axis;
    Vec3 y_axis;
    Vec3 z_axis;
    quat_to_axes(body.rotation, x_axis, y_axis, z_axis);

    const float matrix[16] = {
        x_axis.x, x_axis.y, x_axis.z, 0.0f,
        y_axis.x, y_axis.y, y_axis.z, 0.0f,
        z_axis.x, z_axis.y, z_axis.z, 0.0f,
        body.position.x, body.position.y, body.position.z, 1.0f,
    };

    rlPushMatrix();
    rlMultMatrixf(matrix);
    DrawCube({0.0f, 0.0f, 0.0f}, body.size.x, body.size.y, body.size.z, to_color(body.color));
    const Color wire = selected ? Color{232, 158, 62, 255}
                                : Color{
                                      static_cast<unsigned char>(std::clamp(body.color.r * 0.55f + 0.45f, 0.0f, 1.0f) * 255.0f),
                                      static_cast<unsigned char>(std::clamp(body.color.g * 0.55f + 0.45f, 0.0f, 1.0f) * 255.0f),
                                      static_cast<unsigned char>(std::clamp(body.color.b * 0.55f + 0.45f, 0.0f, 1.0f) * 255.0f),
                                      255,
                                  };
    DrawCubeWires({0.0f, 0.0f, 0.0f}, body.size.x, body.size.y, body.size.z, wire);
    rlPopMatrix();
}

} // namespace

void Renderer::init()
{
    const int fails = view_self_check();
    if (fails != 0)
        std::fprintf(stderr, "view_self_check: %d checks failed\n", fails);

    target_ = LoadRenderTexture(16, 16);
    reset_view();
}

void Renderer::shutdown()
{
    if (target_.id != 0)
    {
        UnloadRenderTexture(target_);
        target_ = {};
    }
}

void Renderer::set_viewport_size(int width, int height)
{
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (target_.id != 0 && target_.texture.width == width && target_.texture.height == height)
        return;

    if (target_.id != 0)
        UnloadRenderTexture(target_);

    target_ = LoadRenderTexture(width, height);
}

void Renderer::reset_view()
{
    view_ = {};
    view_look_at(view_, {0.0f, 1.0f, 0.0f}, {4.0f, 4.0f, 4.0f}, {0.0f, 1.0f, 0.0f});
    view_match_ortho_scale(view_);
    smooth_ = {};
}

void Renderer::cancel_view_motion()
{
    smooth_.active = false;
}

void Renderer::begin_smooth(const View3D& goal)
{
    smooth_.from = view_;
    smooth_.to = goal;
    smooth_.t = 0.0f;
    smooth_.active = true;
    view_.projection = goal.projection;
    view_.view = goal.view;
}

void Renderer::tick_view(float dt)
{
    if (!smooth_.active)
        return;

    smooth_.t += std::max(dt, 0.0f);
    const float u = std::min(smooth_.t / smooth_.duration, 1.0f);
    const float s = u * u * (3.0f - 2.0f * u);
    view_.viewquat = quat_slerp(smooth_.from.viewquat, smooth_.to.viewquat, s);
    view_.ofs = vec_lerp(smooth_.from.ofs, smooth_.to.ofs, s);
    view_.dist = smooth_.from.dist + (smooth_.to.dist - smooth_.from.dist) * s;
    view_.ortho_height = smooth_.from.ortho_height + (smooth_.to.ortho_height - smooth_.from.ortho_height) * s;
    view_.fovy_deg = smooth_.to.fovy_deg;
    view_.projection = smooth_.to.projection;
    view_.view = smooth_.to.view;
    if (u >= 1.0f)
        smooth_.active = false;
}

void Renderer::orbit(float dx_px, float dy_px)
{
    if (dx_px == 0.0f && dy_px == 0.0f)
        return;
    cancel_view_motion();
    view_orbit(view_, -dx_px * 0.005f, dy_px * 0.005f);
}

void Renderer::pan(float dx_px, float dy_px)
{
    if (dx_px == 0.0f && dy_px == 0.0f)
        return;
    cancel_view_motion();
    view_pan(view_, dx_px, dy_px);
}

void Renderer::zoom_at(float wheel_ticks, float ndc_x, float ndc_y, float aspect)
{
    if (wheel_ticks == 0.0f)
        return;
    cancel_view_motion();
    view_zoom_at(view_, wheel_ticks, ndc_x, ndc_y, aspect);
}

void Renderer::set_axis(ViewAxis axis, bool smooth)
{
    View3D goal = view_;
    view_set_axis(goal, axis);
    if (!smooth)
    {
        view_ = goal;
        cancel_view_motion();
        return;
    }
    begin_smooth(goal);
}

void Renderer::toggle_projection()
{
    cancel_view_motion();
    view_toggle_projection(view_);
}

void Renderer::orbit_step(float yaw_radians, float pitch_radians, bool smooth)
{
    View3D goal = view_;
    view_orbit(goal, yaw_radians, pitch_radians);
    if (!smooth)
    {
        view_ = goal;
        cancel_view_motion();
        return;
    }
    begin_smooth(goal);
}

void Renderer::frame_bounds(Vec3 center, float radius, float aspect, bool smooth)
{
    View3D goal = view_;
    view_frame(goal, center, radius, aspect);
    if (!smooth)
    {
        view_ = goal;
        cancel_view_motion();
        return;
    }
    begin_smooth(goal);
}

void Renderer::sync_camera()
{
    camera_.position = to_vec3(view_eye(view_));
    camera_.target = to_vec3(view_pivot(view_));
    camera_.up = to_vec3(view_up(view_));
    if (view_.projection == ViewProjection::Perspective)
    {
        camera_.fovy = view_.fovy_deg;
        camera_.projection = CAMERA_PERSPECTIVE;
    }
    else
    {
        camera_.fovy = view_.ortho_height;
        camera_.projection = CAMERA_ORTHOGRAPHIC;
    }
}

void Renderer::render(const ISimulation& simulation)
{
    if (target_.id == 0)
        return;

    const SimulationState& state = simulation.state();
    const RigidBody body = simulation.visual_body();

    rlSetClipPlanes(view_.clip_near, view_.clip_far);
    sync_camera();

    BeginTextureMode(target_);
    ClearBackground(to_color(state.clear_color));
    BeginMode3D(camera_);

    if (state.show_grid)
        DrawGrid(state.grid_slices, state.grid_spacing);

    if (state.cube_visible)
        draw_body(body, state.cube_selected);

    EndMode3D();
    EndTextureMode();
}

void Renderer::draw_viewport_image()
{
    if (target_.id == 0)
        return;
    rlImGuiImageRenderTexture(&target_);
}

} // namespace openphysx
