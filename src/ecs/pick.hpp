#pragma once

#include "core/Math.h"
#include "core/Types.h"
#include "core/View.h"

#include <algorithm>
#include <cmath>

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

} // namespace openphysx
