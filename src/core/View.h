#pragma once

#include "core/Math.h"

#include <cmath>
#include <cstdio>

namespace openphysx {

// Y-up, right-handed. The camera looks down its local -Z.
// `ofs` is the negative of the orbit pivot, matching Blender's RegionView3D.
// Eye = pivot + rotate(viewquat, (0, 0, dist)). Local +Z points from the pivot back to the eye.
enum class ViewProjection
{
    Perspective,
    Orthographic,
};

enum class ViewAxis
{
    User,
    Front,
    Back,
    Right,
    Left,
    Top,
    Bottom,
};

struct View3D
{
    Quat viewquat{};
    Vec3 ofs{0.0f, -1.0f, 0.0f};
    float dist = 6.403124f;
    float fovy_deg = 45.0f;
    float ortho_height = 5.3f;
    float clip_near = 0.01f;
    float clip_far = 2000.0f;
    ViewProjection projection = ViewProjection::Perspective;
    ViewAxis view = ViewAxis::User;
};

inline Vec3 view_pivot(const View3D& view)
{
    return vec_neg(view.ofs);
}

inline Vec3 view_right(const View3D& view)
{
    return quat_rotate(view.viewquat, {1.0f, 0.0f, 0.0f});
}

inline Vec3 view_up(const View3D& view)
{
    return quat_rotate(view.viewquat, {0.0f, 1.0f, 0.0f});
}

inline Vec3 view_back(const View3D& view)
{
    return quat_rotate(view.viewquat, {0.0f, 0.0f, 1.0f});
}

inline Vec3 view_forward(const View3D& view)
{
    return vec_neg(view_back(view));
}

inline Vec3 view_eye(const View3D& view)
{
    return vec_add(view_pivot(view), vec_scale(view_back(view), view.dist));
}

inline void view_set_pivot(View3D& view, Vec3 pivot)
{
    view.ofs = vec_neg(pivot);
}

inline void view_look_at(View3D& view, Vec3 pivot, Vec3 eye, Vec3 world_up)
{
    const Vec3 back = vec_normalize(vec_sub(eye, pivot));
    const Vec3 forward = vec_neg(back);
    const Vec3 right = vec_normalize(vec_cross(forward, world_up));
    const Vec3 up = vec_normalize(vec_cross(right, forward));
    view.viewquat = quat_from_basis(right, up, back);
    view.dist = std::max(vec_length(vec_sub(eye, pivot)), 0.15f);
    view_set_pivot(view, pivot);
    view.view = ViewAxis::User;
}

inline void view_match_ortho_scale(View3D& view)
{
    const float half = deg_to_rad(view.fovy_deg) * 0.5f;
    view.ortho_height = std::max(2.0f * view.dist * std::tan(half), 0.15f);
}

inline void view_set_axis(View3D& view, ViewAxis axis)
{
    Vec3 back{0.0f, 0.0f, 1.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    switch (axis)
    {
    case ViewAxis::Front:
        back = {0.0f, 0.0f, 1.0f};
        up = {0.0f, 1.0f, 0.0f};
        break;
    case ViewAxis::Back:
        back = {0.0f, 0.0f, -1.0f};
        up = {0.0f, 1.0f, 0.0f};
        break;
    case ViewAxis::Right:
        back = {1.0f, 0.0f, 0.0f};
        up = {0.0f, 1.0f, 0.0f};
        break;
    case ViewAxis::Left:
        back = {-1.0f, 0.0f, 0.0f};
        up = {0.0f, 1.0f, 0.0f};
        break;
    case ViewAxis::Top:
        back = {0.0f, 1.0f, 0.0f};
        up = {0.0f, 0.0f, -1.0f};
        break;
    case ViewAxis::Bottom:
        back = {0.0f, -1.0f, 0.0f};
        up = {0.0f, 0.0f, 1.0f};
        break;
    case ViewAxis::User:
        return;
    }

    const Vec3 forward = vec_neg(back);
    const Vec3 right = vec_normalize(vec_cross(forward, up));
    const Vec3 cam_up = vec_normalize(vec_cross(right, forward));
    view.viewquat = quat_from_basis(right, cam_up, back);
    view.view = axis;
}

inline void view_toggle_projection(View3D& view)
{
    const float half = deg_to_rad(view.fovy_deg) * 0.5f;
    const float tan_half = std::tan(half);
    if (view.projection == ViewProjection::Perspective)
    {
        view.ortho_height = std::clamp(2.0f * view.dist * tan_half, 0.15f, 800.0f);
        view.projection = ViewProjection::Orthographic;
    }
    else
    {
        view.dist = std::clamp(view.ortho_height / (2.0f * tan_half), 0.15f, 800.0f);
        view.projection = ViewProjection::Perspective;
    }
}

// Positive yaw orbits the eye toward +X from the front view (numpad 6).
// Positive pitch raises the eye. Turntable: yaw is world up, pitch is camera right, elevation clamped.
inline void view_orbit(View3D& view, float yaw, float pitch)
{
    if (view.view != ViewAxis::User && view.projection == ViewProjection::Orthographic)
        view_toggle_projection(view);

    if (yaw != 0.0f)
        view.viewquat = quat_normalize(quat_mul(quat_axis_angle({0.0f, 1.0f, 0.0f}, yaw), view.viewquat));

    const Vec3 back = view_back(view);
    const float elev = std::asin(std::clamp(back.y, -1.0f, 1.0f));
    constexpr float kMaxElev = 1.45f;
    const float applied = std::clamp(elev + pitch, -kMaxElev, kMaxElev) - elev;
    if (std::fabs(applied) > 1.0e-6f)
    {
        const Vec3 right = view_right(view);
        view.viewquat = quat_normalize(quat_mul(quat_axis_angle(right, -applied), view.viewquat));
    }
    view.view = ViewAxis::User;
}

inline void view_pan(View3D& view, float dx_px, float dy_px)
{
    const float scale =
        (view.projection == ViewProjection::Perspective ? view.dist : view.ortho_height) * 0.0015f;
    Vec3 pivot = view_pivot(view);
    pivot = vec_add(pivot, vec_scale(view_right(view), -dx_px * scale));
    pivot = vec_add(pivot, vec_scale(view_up(view), dy_px * scale));
    view_set_pivot(view, pivot);
}

inline Vec3 view_cursor_on_pivot_plane(const View3D& view, float ndc_x, float ndc_y, float aspect)
{
    const Vec3 pivot = view_pivot(view);
    const Vec3 forward = view_forward(view);
    const Vec3 right = view_right(view);
    const Vec3 up = view_up(view);
    const float safe_aspect = std::max(aspect, 0.01f);

    if (view.projection == ViewProjection::Orthographic)
    {
        const float half_h = view.ortho_height * 0.5f;
        return vec_add(pivot, vec_add(vec_scale(right, ndc_x * half_h * safe_aspect), vec_scale(up, ndc_y * half_h)));
    }

    const float tan_half = std::tan(deg_to_rad(view.fovy_deg) * 0.5f);
    const Vec3 ray = vec_normalize(vec_add(
        forward,
        vec_add(vec_scale(right, ndc_x * tan_half * safe_aspect), vec_scale(up, ndc_y * tan_half))));
    const float denom = vec_dot(ray, forward);
    if (std::fabs(denom) < 1.0e-5f)
        return pivot;
    return vec_add(view_eye(view), vec_scale(ray, view.dist / denom));
}

// Positive wheel zooms in. The world point under (ndc_x, ndc_y) stays under the cursor.
inline void view_zoom_at(View3D& view, float wheel_ticks, float ndc_x, float ndc_y, float aspect)
{
    if (wheel_ticks == 0.0f)
        return;

    const float factor = std::pow(0.9f, wheel_ticks);
    const Vec3 pivot = view_pivot(view);
    const Vec3 hit = view_cursor_on_pivot_plane(view, ndc_x, ndc_y, aspect);

    if (view.projection == ViewProjection::Perspective)
    {
        const float new_dist = std::clamp(view.dist * factor, 0.15f, 800.0f);
        const float scale = new_dist / view.dist;
        view.dist = new_dist;
        view_set_pivot(view, vec_sub(hit, vec_scale(vec_sub(hit, pivot), scale)));
    }
    else
    {
        const float new_height = std::clamp(view.ortho_height * factor, 0.15f, 800.0f);
        const float scale = new_height / view.ortho_height;
        view.ortho_height = new_height;
        view_set_pivot(view, vec_sub(hit, vec_scale(vec_sub(hit, pivot), scale)));
    }
}

inline void view_frame(View3D& view, Vec3 center, float radius, float aspect)
{
    view_set_pivot(view, center);
    const float half = deg_to_rad(view.fovy_deg) * 0.5f;
    const float sine = std::max(std::sin(half), 1.0e-3f);
    float dist = std::max(radius, 0.05f) / sine;
    if (aspect < 1.0f)
        dist /= std::max(aspect, 0.01f);

    if (view.projection == ViewProjection::Perspective)
        view.dist = std::clamp(dist, 0.15f, 800.0f);
    else
        view.ortho_height = std::clamp(2.0f * dist * std::tan(half), 0.15f, 800.0f);
}

inline bool view_project(const View3D& view, Vec3 world, float aspect, float& ndc_x, float& ndc_y)
{
    const float safe_aspect = std::max(aspect, 0.01f);
    const Vec3 rel = vec_sub(world, view_eye(view));
    const float vx = vec_dot(rel, view_right(view));
    const float vy = vec_dot(rel, view_up(view));
    const float vz = vec_dot(rel, view_forward(view));
    if (vz <= 0.05f)
        return false;

    if (view.projection == ViewProjection::Perspective)
    {
        const float tan_half = std::tan(deg_to_rad(view.fovy_deg) * 0.5f);
        ndc_x = (vx / vz) / (tan_half * safe_aspect);
        ndc_y = (vy / vz) / tan_half;
    }
    else
    {
        const float half_h = view.ortho_height * 0.5f;
        ndc_x = vx / (half_h * safe_aspect);
        ndc_y = vy / half_h;
    }
    return true;
}

inline const char* view_axis_name(ViewAxis axis)
{
    switch (axis)
    {
    case ViewAxis::Front:
        return "Front";
    case ViewAxis::Back:
        return "Back";
    case ViewAxis::Right:
        return "Right";
    case ViewAxis::Left:
        return "Left";
    case ViewAxis::Top:
        return "Top";
    case ViewAxis::Bottom:
        return "Bottom";
    case ViewAxis::User:
        return "User";
    }
    return "User";
}

inline int view_self_check()
{
    int fails = 0;
#define VIEW_CHECK(cond)                                                                 \
    do                                                                                   \
    {                                                                                    \
        if (!(cond))                                                                     \
        {                                                                                \
            std::fprintf(stderr, "view_self_check failed: %s\n", #cond);                \
            ++fails;                                                                     \
        }                                                                                \
    } while (0)

    const Vec3 right = vec_cross({0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});
    VIEW_CHECK(std::fabs(right.x - 1.0f) < 1.0e-5f && std::fabs(right.y) < 1.0e-5f && std::fabs(right.z) < 1.0e-5f);

    const Vec3 turned = quat_rotate(quat_axis_angle({0.0f, 1.0f, 0.0f}, kPi * 0.5f), {0.0f, 0.0f, 1.0f});
    VIEW_CHECK(std::fabs(turned.x - 1.0f) < 1.0e-4f && std::fabs(turned.y) < 1.0e-4f && std::fabs(turned.z) < 1.0e-4f);

    View3D view{};
    const Vec3 pivot{0.0f, 1.0f, 0.0f};
    const Vec3 eye{4.0f, 4.0f, 4.0f};
    view_look_at(view, pivot, eye, {0.0f, 1.0f, 0.0f});
    const Vec3 rebuilt = view_eye(view);
    VIEW_CHECK(vec_length(vec_sub(rebuilt, eye)) < 1.0e-4f);
    const Vec3 to_pivot = vec_normalize(vec_sub(pivot, rebuilt));
    VIEW_CHECK(vec_dot(view_forward(view), to_pivot) > 0.999f);
    VIEW_CHECK(vec_dot(view_up(view), {0.0f, 1.0f, 0.0f}) > 0.2f);

    View3D front{};
    front.dist = 10.0f;
    view_set_pivot(front, pivot);
    view_set_axis(front, ViewAxis::Front);
    const Vec3 front_eye = view_eye(front);
    VIEW_CHECK(std::fabs(front_eye.x - pivot.x) < 1.0e-4f);
    VIEW_CHECK(std::fabs(front_eye.y - pivot.y) < 1.0e-4f);
    VIEW_CHECK(std::fabs(front_eye.z - (pivot.z + 10.0f)) < 1.0e-4f);
    VIEW_CHECK(vec_dot(view_forward(front), {0.0f, 0.0f, -1.0f}) > 0.999f);
    VIEW_CHECK(vec_dot(view_up(front), {0.0f, 1.0f, 0.0f}) > 0.999f);

    View3D top = front;
    view_set_axis(top, ViewAxis::Top);
    VIEW_CHECK(vec_dot(view_forward(top), {0.0f, -1.0f, 0.0f}) > 0.999f);
    VIEW_CHECK(vec_dot(view_up(top), {0.0f, 0.0f, -1.0f}) > 0.999f);
    VIEW_CHECK(vec_dot(view_right(top), {1.0f, 0.0f, 0.0f}) > 0.999f);

    View3D orbited = front;
    view_orbit(orbited, 0.2f, 0.0f);
    VIEW_CHECK(view_eye(orbited).x > pivot.x + 0.5f);

    View3D raised = front;
    view_orbit(raised, 0.0f, 0.3f);
    VIEW_CHECK(view_eye(raised).y > pivot.y + 1.0f);

    View3D panned = front;
    view_pan(panned, 10.0f, 0.0f);
    VIEW_CHECK(view_pivot(panned).x < pivot.x);

    View3D zoomed = front;
    view_zoom_at(zoomed, 1.0f, 0.5f, 0.0f, 1.0f);
    VIEW_CHECK(zoomed.dist < front.dist);
    VIEW_CHECK(view_pivot(zoomed).x > pivot.x);

    float ndc_x = 1.0f;
    float ndc_y = 1.0f;
    VIEW_CHECK(view_project(front, pivot, 1.0f, ndc_x, ndc_y));
    VIEW_CHECK(std::fabs(ndc_x) < 1.0e-4f && std::fabs(ndc_y) < 1.0e-4f);
    VIEW_CHECK(view_project(front, vec_add(pivot, {1.0f, 0.0f, 0.0f}), 1.0f, ndc_x, ndc_y));
    VIEW_CHECK(ndc_x > 0.0f);

    const Quat spun = quat_from_euler_xyz({0.4f, -0.7f, 0.2f});
    const Quat roundtrip = quat_from_euler_xyz(quat_to_euler_xyz(spun));
    const Vec3 sample{0.3f, -0.4f, 0.8f};
    VIEW_CHECK(vec_length(vec_sub(quat_rotate(spun, sample), quat_rotate(roundtrip, sample))) < 1.0e-4f);

    View3D toggled = front;
    toggled.fovy_deg = 90.0f;
    toggled.dist = 10.0f;
    view_toggle_projection(toggled);
    VIEW_CHECK(toggled.projection == ViewProjection::Orthographic);
    VIEW_CHECK(std::fabs(toggled.ortho_height - 20.0f) < 1.0e-3f);
    view_toggle_projection(toggled);
    VIEW_CHECK(toggled.projection == ViewProjection::Perspective);
    VIEW_CHECK(std::fabs(toggled.dist - 10.0f) < 1.0e-3f);

#undef VIEW_CHECK
    return fails;
}

} // namespace openphysx
