#pragma once

#include "core/Math.h"
#include "ecs/physics_ops.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace openphysx {

inline constexpr float kContactSlop = 0.005f;
inline constexpr float kBaumgarte = 0.2f;
inline constexpr float kRestitutionThreshold = 1.0f;
inline constexpr float kSleepLin = 0.08f;
inline constexpr float kSleepAng = 0.15f;
inline constexpr float kSleepTime = 0.4f;

// Normal is unit, from body A toward body B. Pair order is UUID(A) < UUID(B).
struct ContactPoint
{
    UUID a = kNullUuid;
    UUID b = kNullUuid;
    Vec3 point{};
    Vec3 normal{};
    float penetration = 0.0f;
    float friction = 0.5f;
    float restitution = 0.0f;
};

struct PhysicsScratch
{
    struct Body
    {
        UUID id = kNullUuid;
        BodyType type = BodyType::Dynamic;
        ShapeType shape = ShapeType::Box;
        Vec3 center{};
        Quat rotation{};
        Vec3 half{};
        float radius = 0.0f;
        float inv_mass = 0.0f;
        Vec3 inv_inertia{};
        float friction = 0.5f;
        float restitution = 0.0f;
        std::uint32_t layer = 1;
        std::uint32_t mask = 0xffffffffu;
        PhysicsBodyHandle* handle = nullptr;
        const RigidBodyComponent* authored = nullptr;
    };

    struct Aabb
    {
        Vec3 min{};
        Vec3 max{};
    };

    struct Pair
    {
        int i = 0;
        int j = 0;
    };

    struct Manifold
    {
        int i = 0;
        int j = 0;
        Vec3 point{};
        Vec3 normal{};
        float penetration = 0.0f;
        float friction = 0.5f;
        float restitution = 0.0f;
        float lambda_n = 0.0f;
        float lambda_t1 = 0.0f;
        float lambda_t2 = 0.0f;
    };

    std::vector<Body> bodies;
    std::vector<Aabb> aabbs;
    std::vector<Pair> pairs;
    std::vector<Manifold> contacts;
    std::vector<int> uf_parent;
    std::vector<int> uf_rank;

    void ensure(std::size_t n)
    {
        bodies.reserve(n);
        aabbs.reserve(n);
        const std::size_t pairs_n = n * (n + 1) / 2 + 1;
        pairs.reserve(pairs_n);
        contacts.reserve(n * 8 + 8);
        uf_parent.reserve(n);
        uf_rank.reserve(n);
    }

    void begin_frame()
    {
        bodies.clear();
        aabbs.clear();
        pairs.clear();
        contacts.clear();
        uf_parent.clear();
        uf_rank.clear();
    }
};

inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

inline Vec3 vec_mul(Vec3 a, Vec3 b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

inline Vec3 abs_vec(Vec3 v)
{
    return {std::fabs(v.x), std::fabs(v.y), std::fabs(v.z)};
}

inline bool AabbOverlap(const PhysicsScratch::Aabb& a, const PhysicsScratch::Aabb& b)
{
    return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

inline PhysicsScratch::Aabb ObbAabb(Vec3 center, Quat rotation, Vec3 half)
{
    Vec3 x;
    Vec3 y;
    Vec3 z;
    quat_to_axes(rotation, x, y, z);
    const Vec3 e{
        std::fabs(x.x) * half.x + std::fabs(y.x) * half.y + std::fabs(z.x) * half.z,
        std::fabs(x.y) * half.x + std::fabs(y.y) * half.y + std::fabs(z.y) * half.z,
        std::fabs(x.z) * half.x + std::fabs(y.z) * half.y + std::fabs(z.z) * half.z,
    };
    return {vec_sub(center, e), vec_add(center, e)};
}

inline Vec3 ClosestPointOnObb(Vec3 p, Vec3 center, Quat rotation, Vec3 half)
{
    Vec3 x;
    Vec3 y;
    Vec3 z;
    quat_to_axes(rotation, x, y, z);
    const Vec3 d = vec_sub(p, center);
    Vec3 q = center;
    q = vec_add(q, vec_scale(x, clampf(vec_dot(d, x), -half.x, half.x)));
    q = vec_add(q, vec_scale(y, clampf(vec_dot(d, y), -half.y, half.y)));
    q = vec_add(q, vec_scale(z, clampf(vec_dot(d, z), -half.z, half.z)));
    return q;
}

inline void FaceVertices(Vec3 center, Vec3 x, Vec3 y, Vec3 z, Vec3 half, int axis, float sign, Vec3 out[4])
{
    Vec3 u;
    Vec3 v;
    Vec3 n;
    float hu = 0.0f;
    float hv = 0.0f;
    float hn = 0.0f;
    if (axis == 0)
    {
        n = x;
        u = y;
        v = z;
        hn = half.x;
        hu = half.y;
        hv = half.z;
    }
    else if (axis == 1)
    {
        n = y;
        u = x;
        v = z;
        hn = half.y;
        hu = half.x;
        hv = half.z;
    }
    else
    {
        n = z;
        u = x;
        v = y;
        hn = half.z;
        hu = half.x;
        hv = half.y;
    }
    const Vec3 c = vec_add(center, vec_scale(n, sign * hn));
    out[0] = vec_add(c, vec_add(vec_scale(u, hu), vec_scale(v, hv)));
    out[1] = vec_add(c, vec_add(vec_scale(u, hu), vec_scale(v, -hv)));
    out[2] = vec_add(c, vec_add(vec_scale(u, -hu), vec_scale(v, -hv)));
    out[3] = vec_add(c, vec_add(vec_scale(u, -hu), vec_scale(v, hv)));
}

inline int ClipPlane(const Vec3* in, int n, Vec3 plane_n, float plane_d, Vec3* out)
{
    int m = 0;
    if (n <= 0)
        return 0;
    Vec3 prev = in[n - 1];
    float prev_d = vec_dot(prev, plane_n) - plane_d;
    for (int i = 0; i < n; ++i)
    {
        const Vec3 cur = in[i];
        const float cur_d = vec_dot(cur, plane_n) - plane_d;
        if (cur_d <= 0.0f)
        {
            if (prev_d > 0.0f)
            {
                const float t = prev_d / (prev_d - cur_d);
                out[m++] = vec_add(prev, vec_scale(vec_sub(cur, prev), t));
            }
            out[m++] = cur;
        }
        else if (prev_d <= 0.0f)
        {
            const float t = prev_d / (prev_d - cur_d);
            out[m++] = vec_add(prev, vec_scale(vec_sub(cur, prev), t));
        }
        prev = cur;
        prev_d = cur_d;
        if (m >= 8)
            break;
    }
    return m;
}

inline void ProjectObb(Vec3 center, Vec3 x, Vec3 y, Vec3 z, Vec3 half, Vec3 axis, float& lo, float& hi)
{
    const float c = vec_dot(center, axis);
    const float r = std::fabs(vec_dot(x, axis)) * half.x + std::fabs(vec_dot(y, axis)) * half.y +
                    std::fabs(vec_dot(z, axis)) * half.z;
    lo = c - r;
    hi = c + r;
}

inline bool PushContact(PhysicsScratch& scratch, PhysicsScratch::Manifold man)
{
    if (scratch.contacts.size() >= scratch.contacts.capacity())
        return false;
    scratch.contacts.push_back(man);
    return true;
}

inline int ContactSphereSphere(const PhysicsScratch::Body& a, const PhysicsScratch::Body& b, PhysicsScratch::Manifold& out)
{
    const Vec3 d = vec_sub(b.center, a.center);
    const float dist = vec_length(d);
    const float limit = a.radius + b.radius;
    if (dist > limit)
        return 0;
    Vec3 n = dist > 1.0e-8f ? vec_scale(d, 1.0f / dist) : Vec3{0.0f, 1.0f, 0.0f};
    out.point = vec_add(a.center, vec_scale(n, a.radius));
    out.normal = n;
    out.penetration = limit - dist;
    return 1;
}

inline int ContactBoxSphere(
    const PhysicsScratch::Body& box, const PhysicsScratch::Body& sphere, bool sphere_is_b, PhysicsScratch::Manifold& out)
{
    const Vec3 closest = ClosestPointOnObb(sphere.center, box.center, box.rotation, box.half);
    Vec3 d = sphere_is_b ? vec_sub(sphere.center, closest) : vec_sub(closest, sphere.center);
    const float dist = vec_length(d);
    if (dist > sphere.radius && dist > 1.0e-8f)
    {
        // Sphere center outside; still colliding if dist < radius.
    }
    if (dist > sphere.radius)
        return 0;
    Vec3 n;
    if (dist > 1.0e-8f)
        n = vec_scale(d, 1.0f / dist);
    else
        n = sphere_is_b ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{0.0f, -1.0f, 0.0f};
    out.point = closest;
    out.normal = n;
    out.penetration = sphere.radius - dist;
    return 1;
}

inline int ContactBoxBox(
    const PhysicsScratch::Body& a, const PhysicsScratch::Body& b, PhysicsScratch::Manifold* out, int max_out)
{
    Vec3 ax;
    Vec3 ay;
    Vec3 az;
    Vec3 bx;
    Vec3 by;
    Vec3 bz;
    quat_to_axes(a.rotation, ax, ay, az);
    quat_to_axes(b.rotation, bx, by, bz);
    const Vec3 axes[15] = {
        ax, ay, az, bx, by, bz,
        vec_cross(ax, bx), vec_cross(ax, by), vec_cross(ax, bz),
        vec_cross(ay, bx), vec_cross(ay, by), vec_cross(ay, bz),
        vec_cross(az, bx), vec_cross(az, by), vec_cross(az, bz),
    };

    float best = 1.0e9f;
    Vec3 best_n{};
    int best_axis = 0;
    for (int i = 0; i < 15; ++i)
    {
        Vec3 axis = axes[i];
        const float len2 = vec_dot(axis, axis);
        if (len2 < 1.0e-8f)
            continue;
        axis = vec_scale(axis, 1.0f / std::sqrt(len2));
        float alo;
        float ahi;
        float blo;
        float bhi;
        ProjectObb(a.center, ax, ay, az, a.half, axis, alo, ahi);
        ProjectObb(b.center, bx, by, bz, b.half, axis, blo, bhi);
        const float overlap = std::min(ahi, bhi) - std::max(alo, blo);
        if (overlap < 0.0f)
            return 0;
        if (overlap < best)
        {
            best = overlap;
            best_n = axis;
            best_axis = i;
        }
    }

    if (vec_dot(vec_sub(b.center, a.center), best_n) < 0.0f)
        best_n = vec_neg(best_n);

    const bool a_ref = best_axis < 3;
    const PhysicsScratch::Body& ref = a_ref ? a : b;
    const PhysicsScratch::Body& inc = a_ref ? b : a;
    Vec3 rx;
    Vec3 ry;
    Vec3 rz;
    Vec3 ix;
    Vec3 iy;
    Vec3 iz;
    quat_to_axes(ref.rotation, rx, ry, rz);
    quat_to_axes(inc.rotation, ix, iy, iz);
    const Vec3 ref_axes[3] = {rx, ry, rz};
    const Vec3 inc_axes[3] = {ix, iy, iz};
    const int ref_axis = a_ref ? best_axis : best_axis - 3;
    const float ref_sign = vec_dot(ref_axes[ref_axis], best_n) > 0.0f ? 1.0f : -1.0f;
    const Vec3 ref_n = vec_scale(ref_axes[ref_axis], ref_sign);

    int inc_axis = 0;
    float inc_sign = -1.0f;
    float min_dot = 2.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float d = vec_dot(inc_axes[i], ref_n);
        if (d < min_dot)
        {
            min_dot = d;
            inc_axis = i;
            inc_sign = 1.0f;
        }
        if (-d < min_dot)
        {
            min_dot = -d;
            inc_axis = i;
            inc_sign = -1.0f;
        }
    }

    Vec3 incident[4];
    FaceVertices(inc.center, ix, iy, iz, inc.half, inc_axis, inc_sign, incident);

    Vec3 tu;
    Vec3 tv;
    float hu;
    float hv;
    if (ref_axis == 0)
    {
        tu = ry;
        tv = rz;
        hu = ref.half.y;
        hv = ref.half.z;
    }
    else if (ref_axis == 1)
    {
        tu = rx;
        tv = rz;
        hu = ref.half.x;
        hv = ref.half.z;
    }
    else
    {
        tu = rx;
        tv = ry;
        hu = ref.half.x;
        hv = ref.half.y;
    }
    const float du = vec_dot(ref.center, tu);
    const float dv = vec_dot(ref.center, tv);
    Vec3 buf0[8];
    Vec3 buf1[8];
    int n = ClipPlane(incident, 4, tu, du + hu, buf0);
    n = ClipPlane(buf0, n, vec_neg(tu), -du + hu, buf1);
    n = ClipPlane(buf1, n, tv, dv + hv, buf0);
    n = ClipPlane(buf0, n, vec_neg(tv), -dv + hv, buf1);

    int written = 0;
    for (int i = 0; i < n && written < max_out; ++i)
    {
        const float ra = vec_length(a.half) + 0.5f;
        const float rb = vec_length(b.half) + 0.5f;
        if (vec_length(vec_sub(buf1[i], a.center)) > ra && vec_length(vec_sub(buf1[i], b.center)) > rb)
            continue;
        const float side = vec_dot(buf1[i], best_n);
        const float a_hi = vec_dot(a.center, best_n) +
                           std::fabs(vec_dot(ax, best_n)) * a.half.x + std::fabs(vec_dot(ay, best_n)) * a.half.y +
                           std::fabs(vec_dot(az, best_n)) * a.half.z;
        float pen = a_hi - side;
        if (pen < 0.0f)
            continue;
        pen = std::min(pen, best);
        out[written].point = buf1[i];
        out[written].normal = best_n;
        out[written].penetration = pen;
        ++written;
    }
    if (written == 0 && max_out > 0)
    {
        out[0].point = vec_scale(vec_add(a.center, b.center), 0.5f);
        out[0].normal = best_n;
        out[0].penetration = best;
        written = 1;
    }
    return written;
}

inline Vec3 InverseInertia(const PhysicsScratch::Body& b, Vec3 torque)
{
    const Vec3 local = quat_rotate(quat_conjugate(b.rotation), torque);
    const Vec3 scaled = vec_mul(b.inv_inertia, local);
    return quat_rotate(b.rotation, scaled);
}

inline Vec3 VelocityAt(const PhysicsScratch::Body& b, Vec3 p)
{
    const PhysicsBodyHandle& h = *b.handle;
    return vec_add(h.linear_velocity, vec_cross(h.angular_velocity, vec_sub(p, b.center)));
}

inline float EffectiveMass(const PhysicsScratch::Body& a, const PhysicsScratch::Body& b, Vec3 p, Vec3 n)
{
    auto kn = [&](const PhysicsScratch::Body& body) {
        if (body.inv_mass == 0.0f)
            return 0.0f;
        const Vec3 r = vec_sub(p, body.center);
        const Vec3 rxn = vec_cross(r, n);
        return body.inv_mass + vec_dot(rxn, InverseInertia(body, rxn));
    };
    return kn(a) + kn(b);
}

inline void ApplyImpulse(PhysicsScratch::Body& b, Vec3 p, Vec3 impulse)
{
    if (b.inv_mass == 0.0f || b.handle == nullptr || b.authored == nullptr)
        return;
    PhysicsBodyHandle& h = *b.handle;
    h.sleeping = false;
    h.sleep_timer = 0.0f;
    h.linear_velocity = vec_add(h.linear_velocity, vec_mul(vec_scale(impulse, b.inv_mass), b.authored->linearFactor));
    const Vec3 dw = InverseInertia(b, vec_cross(vec_sub(p, b.center), impulse));
    h.angular_velocity = vec_add(h.angular_velocity, vec_mul(dw, b.authored->angularFactor));
}

inline void Tangents(Vec3 n, Vec3& t1, Vec3& t2)
{
    const Vec3 tmp = std::fabs(n.y) < 0.99f ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
    t1 = vec_normalize(vec_cross(n, tmp));
    t2 = vec_cross(n, t1);
}

inline int UfFind(std::vector<int>& parent, int i)
{
    while (parent[static_cast<std::size_t>(i)] != i)
    {
        parent[static_cast<std::size_t>(i)] = parent[static_cast<std::size_t>(parent[static_cast<std::size_t>(i)])];
        i = parent[static_cast<std::size_t>(i)];
    }
    return i;
}

inline void UfUnion(std::vector<int>& parent, std::vector<int>& rank, int a, int b)
{
    a = UfFind(parent, a);
    b = UfFind(parent, b);
    if (a == b)
        return;
    if (rank[static_cast<std::size_t>(a)] < rank[static_cast<std::size_t>(b)])
        parent[static_cast<std::size_t>(a)] = b;
    else if (rank[static_cast<std::size_t>(a)] > rank[static_cast<std::size_t>(b)])
        parent[static_cast<std::size_t>(b)] = a;
    else
    {
        parent[static_cast<std::size_t>(b)] = a;
        ++rank[static_cast<std::size_t>(a)];
    }
}

inline void CollectBodies(Scene& scene, PhysicsScratch& scratch)
{
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity || !entity.Has<RigidBodyComponent>() || !entity.Has<ColliderComponent>())
            continue;
        const ColliderComponent& collider = entity.Get<ColliderComponent>();
        if (collider.isSensor)
            continue;
        if (collider.shape != ShapeType::Box && collider.shape != ShapeType::Sphere)
            continue;
        if (!entity.Has<PhysicsBodyHandle>())
            continue;
        const TransformComponent world = scene.GetWorldTransform(entity);
        const RigidBodyComponent& body = entity.Get<RigidBodyComponent>();
        PhysicsBodyHandle& handle = entity.Get<PhysicsBodyHandle>();
        PhysicsScratch::Body item;
        item.id = id;
        item.type = body.type;
        item.shape = collider.shape;
        item.rotation = quat_mul(handle.rotation, collider.rotation);
        item.center = vec_add(handle.position, quat_rotate(handle.rotation, collider.offset));
        item.half = {
            collider.halfExtents.x * world.scale.x,
            collider.halfExtents.y * world.scale.y,
            collider.halfExtents.z * world.scale.z,
        };
        const float s = std::max(std::fabs(world.scale.x), std::max(std::fabs(world.scale.y), std::fabs(world.scale.z)));
        item.radius = collider.radius * s;
        item.inv_mass = (body.type == BodyType::Dynamic && body.mass > 0.0f) ? 1.0f / body.mass : 0.0f;
        if (item.shape == ShapeType::Sphere)
        {
            const float r2 = item.radius * item.radius;
            const float i = (body.mass > 0.0f && r2 > 1.0e-8f) ? 2.5f / (body.mass * r2) : 0.0f;
            item.inv_inertia = {i, i, i};
        }
        else
        {
            const float hx2 = item.half.x * item.half.x;
            const float hy2 = item.half.y * item.half.y;
            const float hz2 = item.half.z * item.half.z;
            const float m = body.mass;
            auto inv = [&](float a, float b) {
                const float I = (m / 3.0f) * (a + b);
                return (body.type == BodyType::Dynamic && I > 1.0e-8f) ? 1.0f / I : 0.0f;
            };
            item.inv_inertia = {inv(hy2, hz2), inv(hx2, hz2), inv(hx2, hy2)};
        }
        if (body.type != BodyType::Dynamic)
            item.inv_inertia = {};
        item.layer = collider.layer;
        item.mask = collider.mask;
        item.handle = &handle;
        item.authored = &body;
        if (entity.Has<PhysicsMaterialComponent>())
        {
            const PhysicsMaterialComponent& mat = entity.Get<PhysicsMaterialComponent>();
            item.friction = mat.friction;
            item.restitution = mat.restitution;
        }
        if (scratch.bodies.size() < scratch.bodies.capacity())
            scratch.bodies.push_back(item);
    }
    std::sort(scratch.bodies.begin(), scratch.bodies.end(), [](const PhysicsScratch::Body& l, const PhysicsScratch::Body& r) {
        return l.id < r.id;
    });
}

inline void Broadphase(PhysicsScratch& scratch)
{
    scratch.aabbs.resize(scratch.bodies.size());
    for (std::size_t i = 0; i < scratch.bodies.size(); ++i)
    {
        const PhysicsScratch::Body& b = scratch.bodies[i];
        if (b.shape == ShapeType::Sphere)
        {
            const Vec3 e{b.radius, b.radius, b.radius};
            scratch.aabbs[i] = {vec_sub(b.center, e), vec_add(b.center, e)};
        }
        else
            scratch.aabbs[i] = ObbAabb(b.center, b.rotation, b.half);
    }
    for (int i = 0; i < static_cast<int>(scratch.bodies.size()); ++i)
    {
        for (int j = i + 1; j < static_cast<int>(scratch.bodies.size()); ++j)
        {
            const PhysicsScratch::Body& a = scratch.bodies[static_cast<std::size_t>(i)];
            const PhysicsScratch::Body& b = scratch.bodies[static_cast<std::size_t>(j)];
            if (a.inv_mass == 0.0f && b.inv_mass == 0.0f)
                continue;
            if ((a.layer & b.mask) == 0 || (b.layer & a.mask) == 0)
                continue;
            if (!AabbOverlap(scratch.aabbs[static_cast<std::size_t>(i)], scratch.aabbs[static_cast<std::size_t>(j)]))
                continue;
            if (scratch.pairs.size() < scratch.pairs.capacity())
                scratch.pairs.push_back({i, j});
        }
    }
}

inline void Narrowphase(PhysicsScratch& scratch)
{
    for (const PhysicsScratch::Pair& pair : scratch.pairs)
    {
        const PhysicsScratch::Body& a = scratch.bodies[static_cast<std::size_t>(pair.i)];
        const PhysicsScratch::Body& b = scratch.bodies[static_cast<std::size_t>(pair.j)];
        PhysicsScratch::Manifold tmp[8];
        int n = 0;
        if (a.shape == ShapeType::Sphere && b.shape == ShapeType::Sphere)
        {
            n = ContactSphereSphere(a, b, tmp[0]);
        }
        else if (a.shape == ShapeType::Box && b.shape == ShapeType::Sphere)
        {
            n = ContactBoxSphere(a, b, true, tmp[0]);
        }
        else if (a.shape == ShapeType::Sphere && b.shape == ShapeType::Box)
        {
            n = ContactBoxSphere(b, a, false, tmp[0]);
        }
        else if (a.shape == ShapeType::Box && b.shape == ShapeType::Box)
        {
            n = ContactBoxBox(a, b, tmp, 8);
        }
        const float friction = 0.5f * (a.friction + b.friction);
        const float restitution = std::max(a.restitution, b.restitution);
        for (int k = 0; k < n; ++k)
        {
            if (tmp[k].penetration < 0.0f)
                continue;
            tmp[k].i = pair.i;
            tmp[k].j = pair.j;
            if (tmp[k].penetration > 2.0f)
                tmp[k].penetration = 2.0f;
            tmp[k].friction = friction;
            tmp[k].restitution = restitution;
            PushContact(scratch, tmp[k]);
        }
    }
}

inline void WakeIslands(PhysicsScratch& scratch)
{
    const int n = static_cast<int>(scratch.bodies.size());
    scratch.uf_parent.resize(static_cast<std::size_t>(n));
    scratch.uf_rank.resize(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        scratch.uf_parent[static_cast<std::size_t>(i)] = i;
        scratch.uf_rank[static_cast<std::size_t>(i)] = 0;
    }
    for (const PhysicsScratch::Manifold& c : scratch.contacts)
        UfUnion(scratch.uf_parent, scratch.uf_rank, c.i, c.j);

    for (const PhysicsScratch::Manifold& c : scratch.contacts)
    {
        PhysicsScratch::Body& a = scratch.bodies[static_cast<std::size_t>(c.i)];
        PhysicsScratch::Body& b = scratch.bodies[static_cast<std::size_t>(c.j)];
        const bool a_dyn = a.inv_mass > 0.0f;
        const bool b_dyn = b.inv_mass > 0.0f;
        if (a_dyn && b_dyn)
        {
            if (!a.handle->sleeping)
            {
                b.handle->sleeping = false;
                b.handle->sleep_timer = 0.0f;
            }
            if (!b.handle->sleeping)
            {
                a.handle->sleeping = false;
                a.handle->sleep_timer = 0.0f;
            }
        }
        else if (a_dyn && !b.handle->sleeping)
        {
            /* static/kinematic vs sleeping dynamic stays asleep unless the static is kinematic moving */
            if (b.type == BodyType::Kinematic && vec_length(b.handle->linear_velocity) > kSleepLin)
            {
                a.handle->sleeping = false;
                a.handle->sleep_timer = 0.0f;
            }
        }
        else if (b_dyn && !a.handle->sleeping)
        {
            if (a.type == BodyType::Kinematic && vec_length(a.handle->linear_velocity) > kSleepLin)
            {
                b.handle->sleeping = false;
                b.handle->sleep_timer = 0.0f;
            }
        }
        (void)UfFind(scratch.uf_parent, c.i);
    }
}

inline void SolveContacts(PhysicsScratch& scratch, float dt, int iterations)
{
    Vec3 t1;
    Vec3 t2;
    for (int it = 0; it < iterations; ++it)
    {
        for (PhysicsScratch::Manifold& c : scratch.contacts)
        {
            PhysicsScratch::Body& a = scratch.bodies[static_cast<std::size_t>(c.i)];
            PhysicsScratch::Body& b = scratch.bodies[static_cast<std::size_t>(c.j)];
            if (a.handle->sleeping && b.handle->sleeping)
                continue;
            if (a.inv_mass == 0.0f && b.inv_mass == 0.0f)
                continue;

            const Vec3 n = c.normal;
            const Vec3 rel = vec_sub(VelocityAt(b, c.point), VelocityAt(a, c.point));
            const float vn = vec_dot(rel, n);
            const float bias = clampf(-kBaumgarte / dt * std::max(c.penetration - kContactSlop, 0.0f), -4.0f, 0.0f);
            const float e = (vn < -kRestitutionThreshold) ? c.restitution : 0.0f;
            const float kn = EffectiveMass(a, b, c.point, n);
            if (kn <= 1.0e-8f)
                continue;
            float lambda = -(vn + bias + e * vn) / kn;
            const float old = c.lambda_n;
            c.lambda_n = std::max(old + lambda, 0.0f);
            lambda = c.lambda_n - old;
            const Vec3 impulse = vec_scale(n, lambda);
            ApplyImpulse(a, c.point, vec_neg(impulse));
            ApplyImpulse(b, c.point, impulse);

            Tangents(n, t1, t2);
            const Vec3 rel_t = vec_sub(VelocityAt(b, c.point), VelocityAt(a, c.point));
            auto friction_axis = [&](Vec3 t, float& acc) {
                const float vt = vec_dot(rel_t, t);
                const float kt = EffectiveMass(a, b, c.point, t);
                if (kt <= 1.0e-8f)
                    return;
                float lam = -vt / kt;
                const float max_f = c.friction * c.lambda_n;
                const float prev = acc;
                acc = clampf(prev + lam, -max_f, max_f);
                lam = acc - prev;
                const Vec3 j = vec_scale(t, lam);
                ApplyImpulse(a, c.point, vec_neg(j));
                ApplyImpulse(b, c.point, j);
            };
            friction_axis(t1, c.lambda_t1);
            friction_axis(t2, c.lambda_t2);
        }
    }
    for (PhysicsScratch::Body& b : scratch.bodies)
    {
        if (b.handle == nullptr || b.inv_mass == 0.0f)
            continue;
        const float lv = vec_length(b.handle->linear_velocity);
        if (lv > 80.0f)
            b.handle->linear_velocity = vec_scale(b.handle->linear_velocity, 80.0f / lv);
        const float av = vec_length(b.handle->angular_velocity);
        if (av > 40.0f)
            b.handle->angular_velocity = vec_scale(b.handle->angular_velocity, 40.0f / av);
    }
}

inline void UpdateSleep(PhysicsScratch& scratch, float dt)
{
    for (PhysicsScratch::Body& b : scratch.bodies)
    {
        if (b.inv_mass == 0.0f || b.handle == nullptr || b.authored == nullptr)
            continue;
        if (!b.authored->canSleep)
        {
            b.handle->sleeping = false;
            b.handle->sleep_timer = 0.0f;
            continue;
        }
        const float lin = vec_length(b.handle->linear_velocity);
        const float ang = vec_length(b.handle->angular_velocity);
        if (lin < kSleepLin && ang < kSleepAng)
            b.handle->sleep_timer += dt;
        else
        {
            b.handle->sleep_timer = 0.0f;
            b.handle->sleeping = false;
        }
        if (b.handle->sleep_timer >= kSleepTime)
        {
            b.handle->sleeping = true;
            b.handle->linear_velocity = {};
            b.handle->angular_velocity = {};
        }
    }
}

inline void BuildContacts(Scene& scene, PhysicsScratch& scratch)
{
    scratch.begin_frame();
    if (scratch.bodies.capacity() == 0)
        scratch.ensure(std::max<std::size_t>(scene.CreationOrder().size(), 8));
    CollectBodies(scene, scratch);
    Broadphase(scratch);
    Narrowphase(scratch);
}

inline std::vector<ContactPoint> CopyContacts(const PhysicsScratch& scratch)
{
    std::vector<ContactPoint> out;
    out.reserve(scratch.contacts.size());
    for (const PhysicsScratch::Manifold& c : scratch.contacts)
    {
        ContactPoint p;
        p.a = scratch.bodies[static_cast<std::size_t>(c.i)].id;
        p.b = scratch.bodies[static_cast<std::size_t>(c.j)].id;
        p.point = c.point;
        p.normal = c.normal;
        p.penetration = c.penetration;
        p.friction = c.friction;
        p.restitution = c.restitution;
        out.push_back(p);
    }
    return out;
}

inline void SolveSceneContacts(Scene& scene, float dt, PhysicsScratch& scratch)
{
    BuildContacts(scene, scratch);
    WakeIslands(scratch);
    SolveContacts(scratch, dt, std::max(1, scene.world().solver_iterations));
}

} // namespace openphysx
