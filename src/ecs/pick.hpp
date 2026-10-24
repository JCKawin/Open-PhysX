#pragma once

#include "core/Math.h"
#include "core/Types.h"
#include "core/View.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace openphysx {

// A box's footprint on the viewport, in pixels, and its distance from the eye. The nearest
// hit wins when two boxes are under the cursor.
struct ScreenBox
{
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float depth = 0.0f;
};

inline float BodyRadius(const RigidBody& body)
{
    return 0.5f * std::sqrt(body.size.x * body.size.x + body.size.y * body.size.y + body.size.z * body.size.z);
}

// Bounds of the eight corners of the box. False when every corner is behind the camera.
inline bool ProjectBox(const RigidBody& body, const View3D& view, float width, float height, ScreenBox& out)
{
    const float aspect = std::max(width, 1.0f) / std::max(height, 1.0f);
    const Vec3 half = vec_scale(body.size, 0.5f);
    bool any = false;
    out.min_x = 1.0e9f;
    out.min_y = 1.0e9f;
    out.max_x = -1.0e9f;
    out.max_y = -1.0e9f;

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
        if (!view_project(view, world, aspect, ndc_x, ndc_y))
            continue;

        const float x = (ndc_x * 0.5f + 0.5f) * width;
        const float y = (1.0f - (ndc_y * 0.5f + 0.5f)) * height;
        out.min_x = std::min(out.min_x, x);
        out.min_y = std::min(out.min_y, y);
        out.max_x = std::max(out.max_x, x);
        out.max_y = std::max(out.max_y, y);
        any = true;
    }
    out.depth = vec_length(vec_sub(body.position, view_eye(view)));
    return any;
}

inline bool BoxContains(const ScreenBox& box, float x, float y)
{
    return x >= box.min_x && x <= box.max_x && y >= box.min_y && y <= box.max_y;
}

// True when the box touches the rectangle spanned by the two corners.
inline bool BoxOverlaps(const ScreenBox& box, float x0, float y0, float x1, float y1)
{
    return std::min(x0, x1) <= box.max_x && std::max(x0, x1) >= box.min_x && std::min(y0, y1) <= box.max_y &&
           std::max(y0, y1) >= box.min_y;
}

struct Ray3
{
    Vec3 origin{};
    Vec3 direction{};
};

inline Ray3 ViewRay(const View3D& view, float ndc_x, float ndc_y, float aspect)
{
    const float safe_aspect = std::max(aspect, 0.01f);
    const Vec3 forward = view_forward(view);
    const Vec3 right = view_right(view);
    const Vec3 up = view_up(view);
    if (view.projection == ViewProjection::Orthographic)
    {
        const float half_h = view.ortho_height * 0.5f;
        const Vec3 origin = vec_add(
            view_eye(view),
            vec_add(vec_scale(right, ndc_x * half_h * safe_aspect), vec_scale(up, ndc_y * half_h)));
        return {origin, forward};
    }
    const float tan_half = std::tan(deg_to_rad(view.fovy_deg) * 0.5f);
    const Vec3 direction = vec_normalize(vec_add(
        forward,
        vec_add(vec_scale(right, ndc_x * tan_half * safe_aspect), vec_scale(up, ndc_y * tan_half))));
    return {view_eye(view), direction};
}

inline bool IntersectRayPlane(const Ray3& ray, Vec3 point, Vec3 normal, Vec3& hit)
{
    const float denom = vec_dot(ray.direction, normal);
    if (std::fabs(denom) < 1.0e-5f)
        return false;
    const float t = vec_dot(vec_sub(point, ray.origin), normal) / denom;
    if (t < 0.0f)
        return false;
    hit = vec_add(ray.origin, vec_scale(ray.direction, t));
    return true;
}

inline bool IntersectRayObb(const Ray3& ray, const RigidBody& body, Vec3& hit, float& t_hit)
{
    const Quat inv = quat_conjugate(body.rotation);
    const Vec3 origin = quat_rotate(inv, vec_sub(ray.origin, body.position));
    const Vec3 direction = quat_rotate(inv, ray.direction);
    const Vec3 half = vec_scale(body.size, 0.5f);
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {direction.x, direction.y, direction.z};
    const float h[3] = {half.x, half.y, half.z};
    float tmin = 0.0f;
    float tmax = 1.0e9f;
    for (int i = 0; i < 3; ++i)
    {
        if (std::fabs(d[i]) <= 1.0e-8f)
        {
            if (o[i] < -h[i] || o[i] > h[i])
                return false;
            continue;
        }
        float t1 = (-h[i] - o[i]) / d[i];
        float t2 = (h[i] - o[i]) / d[i];
        if (t1 > t2)
            std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax)
            return false;
    }
    if (tmax < 0.0f)
        return false;
    t_hit = tmin >= 0.0f ? tmin : tmax;
    if (t_hit < 0.0f)
        return false;
    hit = vec_add(ray.origin, vec_scale(ray.direction, t_hit));
    return true;
}

inline Vec3 SnapPosition(Vec3 p, float spacing)
{
    const float g = std::max(spacing, 1.0e-4f);
    return {std::round(p.x / g) * g, std::round(p.y / g) * g, std::round(p.z / g) * g};
}

// Object boxes first, then the Y=0 ground plane. False when both miss (ray parallel to
// the ground and no box in front of the camera).
inline bool PlaceCursorOnRay(
    const View3D& view, float ndc_x, float ndc_y, float aspect, const std::vector<RigidBody>& bodies, Vec3& hit)
{
    const Ray3 ray = ViewRay(view, ndc_x, ndc_y, aspect);
    bool any = false;
    float best = 0.0f;
    Vec3 best_hit{};
    for (const RigidBody& body : bodies)
    {
        Vec3 at{};
        float t = 0.0f;
        if (!IntersectRayObb(ray, body, at, t))
            continue;
        if (!any || t < best)
        {
            any = true;
            best = t;
            best_hit = at;
        }
    }
    if (any)
    {
        hit = best_hit;
        return true;
    }
    return IntersectRayPlane(ray, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, hit);
}

} // namespace openphysx
