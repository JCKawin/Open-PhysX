#pragma once

#include "ecs/components/cfd.hpp"
#include "ecs/components/core.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/render.hpp"
#include "ecs/components/robot.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/uuid.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace openphysx {

inline std::string uuid_to_json(UUID id)
{
    return UuidToHex(id);
}

inline UUID uuid_from_json(const nlohmann::json& j, const char* key)
{
    const std::string text = j.value(key, std::string(16, '0'));
    const std::optional<UUID> parsed = UuidFromHex(text);
    return parsed.value_or(kNullUuid);
}

inline std::vector<UUID> uuid_array_from_json(const nlohmann::json& j, const char* key)
{
    std::vector<UUID> ids;
    if (!j.contains(key) || !j.at(key).is_array())
        return ids;
    for (const nlohmann::json& item : j.at(key))
    {
        if (!item.is_string())
            continue;
        const std::optional<UUID> parsed = UuidFromHex(item.get<std::string>());
        if (parsed)
            ids.push_back(*parsed);
    }
    return ids;
}

template<typename Enum>
Enum enum_from_json(const nlohmann::json& j, const char* key, Enum fallback, const std::pair<const char*, Enum>* table, int count)
{
    const std::string text = j.value(key, std::string{});
    for (int i = 0; i < count; ++i)
    {
        if (text == table[i].first)
            return table[i].second;
    }
    return fallback;
}

template<typename Enum>
const char* enum_to_string(Enum value, const std::pair<const char*, Enum>* table, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (value == table[i].second)
            return table[i].first;
    }
    return table[0].first;
}

inline void to_json(nlohmann::json& j, const Vec3& value)
{
    j = nlohmann::json::array({value.x, value.y, value.z});
}

inline void from_json(const nlohmann::json& j, Vec3& value)
{
    value = {};
    if (!j.is_array())
        return;
    if (j.size() > 0 && j.at(0).is_number())
        value.x = j.at(0).get<float>();
    if (j.size() > 1 && j.at(1).is_number())
        value.y = j.at(1).get<float>();
    if (j.size() > 2 && j.at(2).is_number())
        value.z = j.at(2).get<float>();
}

inline void to_json(nlohmann::json& j, const Quat& value)
{
    j = nlohmann::json::array({value.x, value.y, value.z, value.w});
}

inline void from_json(const nlohmann::json& j, Quat& value)
{
    value = {};
    if (!j.is_array())
        return;
    if (j.size() > 0 && j.at(0).is_number())
        value.x = j.at(0).get<float>();
    if (j.size() > 1 && j.at(1).is_number())
        value.y = j.at(1).get<float>();
    if (j.size() > 2 && j.at(2).is_number())
        value.z = j.at(2).get<float>();
    if (j.size() > 3 && j.at(3).is_number())
        value.w = j.at(3).get<float>();
}

inline void to_json(nlohmann::json& j, const Rgb& value)
{
    j = nlohmann::json::array({value.r, value.g, value.b});
}

inline void from_json(const nlohmann::json& j, Rgb& value)
{
    value = {};
    if (!j.is_array())
        return;
    if (j.size() > 0 && j.at(0).is_number())
        value.r = j.at(0).get<float>();
    if (j.size() > 1 && j.at(1).is_number())
        value.g = j.at(1).get<float>();
    if (j.size() > 2 && j.at(2).is_number())
        value.b = j.at(2).get<float>();
}

inline void to_json(nlohmann::json& j, const Mat3& value)
{
    j = nlohmann::json::array();
    for (float item : value.m)
        j.push_back(item);
}

inline void from_json(const nlohmann::json& j, Mat3& value)
{
    value = {};
    if (!j.is_array())
        return;
    const int count = static_cast<int>(j.size() < 9 ? j.size() : 9);
    for (int i = 0; i < count; ++i)
    {
        if (j.at(i).is_number())
            value.m[i] = j.at(i).get<float>();
    }
}

inline void to_json(nlohmann::json& j, const IDComponent& value)
{
    j = nlohmann::json{{"id", uuid_to_json(value.id)}};
}

inline void from_json(const nlohmann::json& j, IDComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.id = uuid_from_json(j, "id");
}

inline void to_json(nlohmann::json& j, const TagComponent& value)
{
    j = nlohmann::json{{"name", value.name}};
}

inline void from_json(const nlohmann::json& j, TagComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.name = j.value("name", std::string("Entity"));
}

inline void to_json(nlohmann::json& j, const TransformComponent& value)
{
    j = nlohmann::json{{"position", value.position}, {"rotation", value.rotation}, {"scale", value.scale}};
}

inline void from_json(const nlohmann::json& j, TransformComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.position = j.value("position", value.position);
    value.rotation = j.value("rotation", value.rotation);
    value.scale = j.value("scale", value.scale);
}

inline void to_json(nlohmann::json& j, const RelationshipComponent& value)
{
    nlohmann::json children = nlohmann::json::array();
    for (const UUID id : value.children)
        children.push_back(uuid_to_json(id));
    j = nlohmann::json{{"parent", uuid_to_json(value.parent)}, {"children", std::move(children)}};
}

inline void from_json(const nlohmann::json& j, RelationshipComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.parent = uuid_from_json(j, "parent");
    value.children = uuid_array_from_json(j, "children");
}

inline void to_json(nlohmann::json& j, const EditorStateComponent& value)
{
    j = nlohmann::json{{"visible", value.visible}, {"locked", value.locked}};
}

inline void from_json(const nlohmann::json& j, EditorStateComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.visible = j.value("visible", true);
    value.locked = j.value("locked", false);
}

inline void to_json(nlohmann::json& j, const PrimitiveBoxComponent& value)
{
    j = nlohmann::json{{"size", value.size}, {"color", value.color}};
}

inline void from_json(const nlohmann::json& j, PrimitiveBoxComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.size = j.value("size", value.size);
    value.color = j.value("color", value.color);
}

inline void to_json(nlohmann::json& j, const MeshRendererComponent& value)
{
    j = nlohmann::json{
        {"meshAsset", uuid_to_json(value.meshAsset)},
        {"materialAsset", uuid_to_json(value.materialAsset)},
        {"castShadow", value.castShadow},
    };
}

inline void from_json(const nlohmann::json& j, MeshRendererComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.meshAsset = uuid_from_json(j, "meshAsset");
    value.materialAsset = uuid_from_json(j, "materialAsset");
    value.castShadow = j.value("castShadow", true);
}

inline void to_json(nlohmann::json& j, const MaterialOverrideComponent& value)
{
    j = nlohmann::json{{"albedo", value.albedo}, {"roughness", value.roughness}, {"metallic", value.metallic}};
}

inline void from_json(const nlohmann::json& j, MaterialOverrideComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.albedo = j.value("albedo", value.albedo);
    value.roughness = j.value("roughness", value.roughness);
    value.metallic = j.value("metallic", value.metallic);
}

inline const std::pair<const char*, BodyType> kBodyTypes[] = {
    {"Static", BodyType::Static},
    {"Kinematic", BodyType::Kinematic},
    {"Dynamic", BodyType::Dynamic},
};

inline const std::pair<const char*, ShapeType> kShapeTypes[] = {
    {"Box", ShapeType::Box},
    {"Sphere", ShapeType::Sphere},
    {"Capsule", ShapeType::Capsule},
    {"Cylinder", ShapeType::Cylinder},
    {"Plane", ShapeType::Plane},
};

inline const std::pair<const char*, JointType> kJointTypes[] = {
    {"Fixed", JointType::Fixed},
    {"Revolute", JointType::Revolute},
    {"Prismatic", JointType::Prismatic},
    {"Spherical", JointType::Spherical},
    {"Cylindrical", JointType::Cylindrical},
    {"SixDof", JointType::SixDof},
};

inline const std::pair<const char*, ActuatorType> kActuatorTypes[] = {
    {"Revolute", ActuatorType::Revolute},
    {"Prismatic", ActuatorType::Prismatic},
    {"Fixed", ActuatorType::Fixed},
};

inline const std::pair<const char*, SensorType> kSensorTypes[] = {
    {"Imu", SensorType::Imu},
    {"Force", SensorType::Force},
    {"JointPosition", SensorType::JointPosition},
    {"Contact", SensorType::Contact},
};

inline const std::pair<const char*, TurbulenceModel> kTurbulenceModels[] = {
    {"None", TurbulenceModel::None},
    {"Laminar", TurbulenceModel::Laminar},
    {"KEpsilon", TurbulenceModel::KEpsilon},
};

inline const std::pair<const char*, BoundaryKind> kBoundaryKinds[] = {
    {"Wall", BoundaryKind::Wall},
    {"Inlet", BoundaryKind::Inlet},
    {"Outlet", BoundaryKind::Outlet},
    {"Symmetry", BoundaryKind::Symmetry},
};

inline void to_json(nlohmann::json& j, const RigidBodyComponent& value)
{
    j = nlohmann::json{
        {"type", enum_to_string(value.type, kBodyTypes, 3)},
        {"mass", value.mass},
        {"inertia", value.inertia},
        {"autoInertia", value.autoInertia},
        {"linearVelocity", value.linearVelocity},
        {"angularVelocity", value.angularVelocity},
        {"linearDamping", value.linearDamping},
        {"angularDamping", value.angularDamping},
        {"gravityEnabled", value.gravityEnabled},
        {"ccd", value.ccd},
    };
}

inline void from_json(const nlohmann::json& j, RigidBodyComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.type = enum_from_json(j, "type", BodyType::Dynamic, kBodyTypes, 3);
    value.mass = j.value("mass", value.mass);
    value.inertia = j.value("inertia", value.inertia);
    value.autoInertia = j.value("autoInertia", value.autoInertia);
    value.linearVelocity = j.value("linearVelocity", value.linearVelocity);
    value.angularVelocity = j.value("angularVelocity", value.angularVelocity);
    value.linearDamping = j.value("linearDamping", value.linearDamping);
    value.angularDamping = j.value("angularDamping", value.angularDamping);
    value.gravityEnabled = j.value("gravityEnabled", value.gravityEnabled);
    value.ccd = j.value("ccd", value.ccd);
}

inline void to_json(nlohmann::json& j, const ColliderComponent& value)
{
    j = nlohmann::json{
        {"shape", enum_to_string(value.shape, kShapeTypes, 5)},
        {"halfExtents", value.halfExtents},
        {"radius", value.radius},
        {"height", value.height},
        {"offset", value.offset},
        {"rotation", value.rotation},
        {"physicsMaterial", uuid_to_json(value.physicsMaterial)},
        {"layer", value.layer},
        {"mask", value.mask},
        {"isSensor", value.isSensor},
    };
}

inline void from_json(const nlohmann::json& j, ColliderComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.shape = enum_from_json(j, "shape", ShapeType::Box, kShapeTypes, 5);
    value.halfExtents = j.value("halfExtents", value.halfExtents);
    value.radius = j.value("radius", value.radius);
    value.height = j.value("height", value.height);
    value.offset = j.value("offset", value.offset);
    value.rotation = j.value("rotation", value.rotation);
    value.physicsMaterial = uuid_from_json(j, "physicsMaterial");
    value.layer = j.value("layer", value.layer);
    value.mask = j.value("mask", value.mask);
    value.isSensor = j.value("isSensor", value.isSensor);
}

inline void to_json(nlohmann::json& j, const PhysicsMaterialComponent& value)
{
    j = nlohmann::json{
        {"friction", value.friction},
        {"restitution", value.restitution},
        {"rollingFriction", value.rollingFriction},
    };
}

inline void from_json(const nlohmann::json& j, PhysicsMaterialComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.friction = j.value("friction", value.friction);
    value.restitution = j.value("restitution", value.restitution);
    value.rollingFriction = j.value("rollingFriction", value.rollingFriction);
}

inline void to_json(nlohmann::json& j, const JointComponent& value)
{
    j = nlohmann::json{
        {"type", enum_to_string(value.type, kJointTypes, 6)},
        {"bodyA", uuid_to_json(value.bodyA)},
        {"bodyB", uuid_to_json(value.bodyB)},
        {"anchorA", value.anchorA},
        {"anchorB", value.anchorB},
        {"axisA", value.axisA},
        {"axisB", value.axisB},
        {"limitMin", value.limitMin},
        {"limitMax", value.limitMax},
        {"limitEnabled", value.limitEnabled},
        {"motorTarget", value.motorTarget},
        {"motorMaxForce", value.motorMaxForce},
        {"motorEnabled", value.motorEnabled},
    };
}

inline void from_json(const nlohmann::json& j, JointComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.type = enum_from_json(j, "type", JointType::Fixed, kJointTypes, 6);
    value.bodyA = uuid_from_json(j, "bodyA");
    value.bodyB = uuid_from_json(j, "bodyB");
    value.anchorA = j.value("anchorA", value.anchorA);
    value.anchorB = j.value("anchorB", value.anchorB);
    value.axisA = j.value("axisA", value.axisA);
    value.axisB = j.value("axisB", value.axisB);
    value.limitMin = j.value("limitMin", value.limitMin);
    value.limitMax = j.value("limitMax", value.limitMax);
    value.limitEnabled = j.value("limitEnabled", value.limitEnabled);
    value.motorTarget = j.value("motorTarget", value.motorTarget);
    value.motorMaxForce = j.value("motorMaxForce", value.motorMaxForce);
    value.motorEnabled = j.value("motorEnabled", value.motorEnabled);
}

inline void to_json(nlohmann::json& j, const RobotComponent& value)
{
    j = nlohmann::json{{"name", value.name}, {"rootLink", uuid_to_json(value.rootLink)}};
}

inline void from_json(const nlohmann::json& j, RobotComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.name = j.value("name", std::string("Robot"));
    value.rootLink = uuid_from_json(j, "rootLink");
}

inline void to_json(nlohmann::json& j, const LinkComponent& value)
{
    j = nlohmann::json{{"robot", uuid_to_json(value.robot)}, {"index", value.index}};
}

inline void from_json(const nlohmann::json& j, LinkComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.robot = uuid_from_json(j, "robot");
    value.index = j.value("index", value.index);
}

inline void to_json(nlohmann::json& j, const ActuatorComponent& value)
{
    j = nlohmann::json{
        {"type", enum_to_string(value.type, kActuatorTypes, 3)},
        {"joint", uuid_to_json(value.joint)},
        {"effortLimit", value.effortLimit},
        {"velocityLimit", value.velocityLimit},
    };
}

inline void from_json(const nlohmann::json& j, ActuatorComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.type = enum_from_json(j, "type", ActuatorType::Revolute, kActuatorTypes, 3);
    value.joint = uuid_from_json(j, "joint");
    value.effortLimit = j.value("effortLimit", value.effortLimit);
    value.velocityLimit = j.value("velocityLimit", value.velocityLimit);
}

inline void to_json(nlohmann::json& j, const SensorComponent& value)
{
    j = nlohmann::json{
        {"type", enum_to_string(value.type, kSensorTypes, 4)},
        {"link", uuid_to_json(value.link)},
        {"frame", value.frame},
    };
}

inline void from_json(const nlohmann::json& j, SensorComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.type = enum_from_json(j, "type", SensorType::JointPosition, kSensorTypes, 4);
    value.link = uuid_from_json(j, "link");
    value.frame = j.value("frame", std::string("base"));
}

inline void to_json(nlohmann::json& j, const BoundaryCondition& value)
{
    j = nlohmann::json{
        {"kind", enum_to_string(value.kind, kBoundaryKinds, 4)},
        {"velocity", value.velocity},
        {"pressure", value.pressure},
    };
}

inline void from_json(const nlohmann::json& j, BoundaryCondition& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.kind = enum_from_json(j, "kind", BoundaryKind::Wall, kBoundaryKinds, 4);
    value.velocity = j.value("velocity", value.velocity);
    value.pressure = j.value("pressure", value.pressure);
}

inline void to_json(nlohmann::json& j, const CfdDomainComponent& value)
{
    j = nlohmann::json{
        {"boundsMin", value.boundsMin},
        {"boundsMax", value.boundsMax},
        {"resolution", nlohmann::json::array({value.resolutionX, value.resolutionY, value.resolutionZ})},
        {"density", value.density},
        {"viscosity", value.viscosity},
        {"turbulence", enum_to_string(value.turbulence, kTurbulenceModels, 3)},
        {"boundaries", value.boundaries},
    };
}

inline void from_json(const nlohmann::json& j, CfdDomainComponent& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.boundsMin = j.value("boundsMin", value.boundsMin);
    value.boundsMax = j.value("boundsMax", value.boundsMax);
    if (j.contains("resolution") && j.at("resolution").is_array())
    {
        const nlohmann::json& resolution = j.at("resolution");
        if (resolution.size() > 0 && resolution.at(0).is_number_integer())
            value.resolutionX = resolution.at(0).get<int>();
        if (resolution.size() > 1 && resolution.at(1).is_number_integer())
            value.resolutionY = resolution.at(1).get<int>();
        if (resolution.size() > 2 && resolution.at(2).is_number_integer())
            value.resolutionZ = resolution.at(2).get<int>();
    }
    value.density = j.value("density", value.density);
    value.viscosity = j.value("viscosity", value.viscosity);
    value.turbulence = enum_from_json(j, "turbulence", TurbulenceModel::Laminar, kTurbulenceModels, 3);
    value.boundaries = j.value("boundaries", value.boundaries);
}

inline void to_json(nlohmann::json& j, const SelectionOutlineTag& value)
{
    j = nlohmann::json{{"marked", value.marked}};
}

inline void from_json(const nlohmann::json& j, SelectionOutlineTag& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.marked = j.value("marked", true);
}

inline void to_json(nlohmann::json& j, const MeshGpuHandle& value)
{
    j = nlohmann::json{{"vao", value.vao}, {"vbo", value.vbo}};
}

inline void from_json(const nlohmann::json& j, MeshGpuHandle& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.vao = j.value("vao", 0u);
    value.vbo = j.value("vbo", 0u);
}

inline void to_json(nlohmann::json& j, const PhysicsBodyHandle& value)
{
    j = nlohmann::json{{"index", value.index}};
}

inline void from_json(const nlohmann::json& j, PhysicsBodyHandle& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.index = j.value("index", -1);
}

inline void to_json(nlohmann::json& j, const ContactCache& value)
{
    j = nlohmann::json{{"count", value.count}};
}

inline void from_json(const nlohmann::json& j, ContactCache& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.count = j.value("count", 0);
}

inline void to_json(nlohmann::json& j, const CfdResultField& value)
{
    j = nlohmann::json{{"cells", value.cells}, {"time", value.time}};
}

inline void from_json(const nlohmann::json& j, CfdResultField& value)
{
    value = {};
    if (!j.is_object())
        return;
    value.cells = j.value("cells", 0);
    value.time = j.value("time", 0.0f);
}

} // namespace openphysx
