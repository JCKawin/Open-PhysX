#pragma once

#include "core/Types.h"
#include "ecs/uuid.hpp"

#include <cstdint>

namespace openphysx {

enum class BodyType
{
    Static,
    Kinematic,
    Dynamic,
};

enum class ShapeType
{
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Plane,
};

enum class JointType
{
    Fixed,
    Revolute,
    Prismatic,
    Spherical,
    Cylindrical,
    SixDof,
};

// Row-major 3x3. Default is identity. Float, matching the rest of the engine.
struct Mat3
{
    float m[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
};

struct RigidBodyComponent
{
    BodyType type = BodyType::Dynamic;
    float mass = 1.0f;
    Mat3 inertia{};
    bool autoInertia = true;
    Vec3 linearVelocity{};
    Vec3 angularVelocity{};
    float linearDamping = 0.0f;
    float angularDamping = 0.05f;
    bool gravityEnabled = true;
    bool ccd = false;
};

// One collider per entity. A body that needs more shapes uses child entities,
// each with its own ColliderComponent. The solver, when it exists, gathers them.
struct ColliderComponent
{
    ShapeType shape = ShapeType::Box;
    Vec3 halfExtents{1.0f, 1.0f, 1.0f};
    float radius = 0.5f;
    float height = 1.0f;
    Vec3 offset{};
    Quat rotation{};
    UUID physicsMaterial = kNullUuid;
    std::uint32_t layer = 1;
    std::uint32_t mask = 0xffffffffu;
    bool isSensor = false;
};

struct PhysicsMaterialComponent
{
    float friction = 0.5f;
    float restitution = 0.0f;
    float rollingFriction = 0.0f;
};

struct JointComponent
{
    JointType type = JointType::Fixed;
    UUID bodyA = kNullUuid;
    UUID bodyB = kNullUuid;
    Vec3 anchorA{};
    Vec3 anchorB{};
    Vec3 axisA{0.0f, 1.0f, 0.0f};
    Vec3 axisB{0.0f, 1.0f, 0.0f};
    float limitMin = 0.0f;
    float limitMax = 0.0f;
    bool limitEnabled = false;
    float motorTarget = 0.0f;
    float motorMaxForce = 0.0f;
    bool motorEnabled = false;
};

inline bool operator==(const Mat3& a, const Mat3& b)
{
    for (int i = 0; i < 9; ++i)
    {
        if (a.m[i] != b.m[i])
            return false;
    }
    return true;
}

inline bool operator==(const RigidBodyComponent& a, const RigidBodyComponent& b)
{
    return a.type == b.type && a.mass == b.mass && a.inertia == b.inertia && a.autoInertia == b.autoInertia &&
           a.linearVelocity == b.linearVelocity && a.angularVelocity == b.angularVelocity &&
           a.linearDamping == b.linearDamping && a.angularDamping == b.angularDamping &&
           a.gravityEnabled == b.gravityEnabled && a.ccd == b.ccd;
}

inline bool operator==(const ColliderComponent& a, const ColliderComponent& b)
{
    return a.shape == b.shape && a.halfExtents == b.halfExtents && a.radius == b.radius && a.height == b.height &&
           a.offset == b.offset && a.rotation == b.rotation && a.physicsMaterial == b.physicsMaterial &&
           a.layer == b.layer && a.mask == b.mask && a.isSensor == b.isSensor;
}

inline bool operator==(const PhysicsMaterialComponent& a, const PhysicsMaterialComponent& b)
{
    return a.friction == b.friction && a.restitution == b.restitution && a.rollingFriction == b.rollingFriction;
}

inline bool operator==(const JointComponent& a, const JointComponent& b)
{
    return a.type == b.type && a.bodyA == b.bodyA && a.bodyB == b.bodyB && a.anchorA == b.anchorA &&
           a.anchorB == b.anchorB && a.axisA == b.axisA && a.axisB == b.axisB && a.limitMin == b.limitMin &&
           a.limitMax == b.limitMax && a.limitEnabled == b.limitEnabled && a.motorTarget == b.motorTarget &&
           a.motorMaxForce == b.motorMaxForce && a.motorEnabled == b.motorEnabled;
}

} // namespace openphysx
