#pragma once

#include "core/Types.h"

#include <algorithm>
#include <cmath>

namespace openphysx {

constexpr float kPi = 3.14159265358979323846f;

inline float deg_to_rad(float deg)
{
    return deg * (kPi / 180.0f);
}

inline float rad_to_deg(float rad)
{
    return rad * (180.0f / kPi);
}

inline Vec3 vec_add(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 vec_sub(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 vec_scale(Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

inline Vec3 vec_neg(Vec3 v)
{
    return {-v.x, -v.y, -v.z};
}

inline float vec_dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 vec_cross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float vec_length(Vec3 v)
{
    return std::sqrt(vec_dot(v, v));
}

inline Vec3 vec_normalize(Vec3 v)
{
    const float length = vec_length(v);
    if (length <= 1.0e-8f)
        return {0.0f, 0.0f, 0.0f};
    return vec_scale(v, 1.0f / length);
}

inline Vec3 vec_lerp(Vec3 a, Vec3 b, float t)
{
    return vec_add(a, vec_scale(vec_sub(b, a), t));
}

inline Quat quat_normalize(Quat q)
{
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length <= 1.0e-8f)
        return {};
    const float inv = 1.0f / length;
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

inline Quat quat_mul(Quat a, Quat b)
{
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

inline Quat quat_axis_angle(Vec3 axis, float radians)
{
    axis = vec_normalize(axis);
    const float half = radians * 0.5f;
    const float s = std::sin(half);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
}

inline Vec3 quat_rotate(Quat q, Vec3 v)
{
    const Quat p{v.x, v.y, v.z, 0.0f};
    const Quat inv{-q.x, -q.y, -q.z, q.w};
    const Quat r = quat_mul(quat_mul(q, p), inv);
    return {r.x, r.y, r.z};
}

// Column-vector matrix. Columns are the images of the local axes.
inline Quat quat_from_basis(Vec3 x_axis, Vec3 y_axis, Vec3 z_axis)
{
    const float m00 = x_axis.x;
    const float m01 = y_axis.x;
    const float m02 = z_axis.x;
    const float m10 = x_axis.y;
    const float m11 = y_axis.y;
    const float m12 = z_axis.y;
    const float m20 = x_axis.z;
    const float m21 = y_axis.z;
    const float m22 = z_axis.z;

    Quat q;
    const float trace = m00 + m11 + m22;
    if (trace > 0.0f)
    {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    }
    else if (m00 > m11 && m00 > m22)
    {
        const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    }
    else if (m11 > m22)
    {
        const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    }
    else
    {
        const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }
    return quat_normalize(q);
}

inline void quat_to_axes(Quat q, Vec3& x_axis, Vec3& y_axis, Vec3& z_axis)
{
    x_axis = quat_rotate(q, {1.0f, 0.0f, 0.0f});
    y_axis = quat_rotate(q, {0.0f, 1.0f, 0.0f});
    z_axis = quat_rotate(q, {0.0f, 0.0f, 1.0f});
}

// Intrinsic XYZ: v' = Rz * Ry * Rx * v.
inline Quat quat_from_euler_xyz(Vec3 radians)
{
    const Quat qx = quat_axis_angle({1.0f, 0.0f, 0.0f}, radians.x);
    const Quat qy = quat_axis_angle({0.0f, 1.0f, 0.0f}, radians.y);
    const Quat qz = quat_axis_angle({0.0f, 0.0f, 1.0f}, radians.z);
    return quat_normalize(quat_mul(qz, quat_mul(qy, qx)));
}

inline Vec3 quat_to_euler_xyz(Quat q)
{
    Vec3 x_axis;
    Vec3 y_axis;
    Vec3 z_axis;
    quat_to_axes(q, x_axis, y_axis, z_axis);

    // Rows of the rotation matrix are the axis components written this way:
    // m[row][col], columns are x_axis, y_axis, z_axis.
    const float m20 = x_axis.z;
    const float m21 = y_axis.z;
    const float m22 = z_axis.z;
    const float m10 = x_axis.y;
    const float m00 = x_axis.x;

    Vec3 euler;
    if (m20 < 0.99999f)
    {
        if (m20 > -0.99999f)
        {
            euler.y = std::asin(std::clamp(-m20, -1.0f, 1.0f));
            euler.x = std::atan2(m21, m22);
            euler.z = std::atan2(m10, m00);
        }
        else
        {
            euler.y = kPi * 0.5f;
            euler.x = std::atan2(-z_axis.y, y_axis.y);
            euler.z = 0.0f;
        }
    }
    else
    {
        euler.y = -kPi * 0.5f;
        euler.x = std::atan2(-z_axis.y, y_axis.y);
        euler.z = 0.0f;
    }
    return euler;
}

inline Quat quat_slerp(Quat a, Quat b, float t)
{
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f)
    {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }

    if (dot > 0.9995f)
    {
        return quat_normalize({
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t,
            a.w + (b.w - a.w) * t,
        });
    }

    const float theta = std::acos(std::clamp(dot, -1.0f, 1.0f));
    const float s = std::sin(theta);
    const float w1 = std::sin((1.0f - t) * theta) / s;
    const float w2 = std::sin(t * theta) / s;
    return {
        a.x * w1 + b.x * w2,
        a.y * w1 + b.y * w2,
        a.z * w1 + b.z * w2,
        a.w * w1 + b.w * w2,
    };
}

} // namespace openphysx
