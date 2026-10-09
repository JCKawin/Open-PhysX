#pragma once

#include "ecs/uuid.hpp"

#include <string>

namespace openphysx {

enum class ActuatorType
{
    Revolute,
    Prismatic,
    Fixed,
};

enum class SensorType
{
    Imu,
    Force,
    JointPosition,
    Contact,
};

struct RobotComponent
{
    std::string name = "Robot";
    UUID rootLink = kNullUuid;
};

struct LinkComponent
{
    UUID robot = kNullUuid;
    int index = 0;
};

struct ActuatorComponent
{
    ActuatorType type = ActuatorType::Revolute;
    UUID joint = kNullUuid;
    float effortLimit = 0.0f;
    float velocityLimit = 0.0f;
};

struct SensorComponent
{
    SensorType type = SensorType::JointPosition;
    UUID link = kNullUuid;
    std::string frame = "base";
};

inline bool operator==(const RobotComponent& a, const RobotComponent& b)
{
    return a.name == b.name && a.rootLink == b.rootLink;
}

inline bool operator==(const LinkComponent& a, const LinkComponent& b)
{
    return a.robot == b.robot && a.index == b.index;
}

inline bool operator==(const ActuatorComponent& a, const ActuatorComponent& b)
{
    return a.type == b.type && a.joint == b.joint && a.effortLimit == b.effortLimit && a.velocityLimit == b.velocityLimit;
}

inline bool operator==(const SensorComponent& a, const SensorComponent& b)
{
    return a.type == b.type && a.link == b.link && a.frame == b.frame;
}

} // namespace openphysx
