#include "persistence/scene_serializer.hpp"

#include "ecs/component_registry.hpp"
#include "ecs/components/cfd.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/render.hpp"
#include "ecs/components/robot.hpp"
#include "ecs/components/serialize.hpp"

#include <cmath>
#include <unordered_set>

namespace openphysx {
namespace {

constexpr int kMaxDepth = 64;

bool depth_ok(const nlohmann::json& json, int depth)
{
    if (depth > kMaxDepth)
        return false;
    if (json.is_array())
    {
        for (const nlohmann::json& item : json)
        {
            if (!depth_ok(item, depth + 1))
                return false;
        }
    }
    else if (json.is_object())
    {
        for (const auto& item : json.items())
        {
            if (!depth_ok(item.value(), depth + 1))
                return false;
        }
    }
    return true;
}

void clear_parent(Scene& scene, Entity entity)
{
    if (!entity || !entity.Has<RelationshipComponent>())
        return;
    RelationshipComponent& link = entity.Get<RelationshipComponent>();
    if (link.parent != kNullUuid)
    {
        Entity parent = scene.FindByUUID(link.parent);
        if (parent && parent.Has<RelationshipComponent>())
            std::erase(parent.Get<RelationshipComponent>().children, entity.GetUUID());
    }
    link.parent = kNullUuid;
}

UUID require_uuid(Scene& scene, UUID id, const char* field, Entity owner, LoadReport& report)
{
    if (id == kNullUuid)
        return kNullUuid;
    if (scene.FindByUUID(id))
        return id;
    report.warning(owner.Get<TagComponent>().name + " " + field + " points at a missing entity and was cleared.");
    return kNullUuid;
}

void resolve_references(Scene& scene, LoadReport& report)
{
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity)
            continue;

        if (entity.Has<RelationshipComponent>())
        {
            RelationshipComponent& link = entity.Get<RelationshipComponent>();
            if (link.parent != kNullUuid && !scene.FindByUUID(link.parent))
            {
                report.warning(entity.Get<TagComponent>().name + " parent is missing and was cleared.");
                link.parent = kNullUuid;
            }
            std::vector<UUID> kept;
            kept.reserve(link.children.size());
            for (const UUID child : link.children)
            {
                if (scene.FindByUUID(child))
                    kept.push_back(child);
                else
                    report.warning(entity.Get<TagComponent>().name + " lists a missing child that was removed.");
            }
            link.children = std::move(kept);
        }

        if (entity.Has<JointComponent>())
        {
            JointComponent& joint = entity.Get<JointComponent>();
            joint.bodyA = require_uuid(scene, joint.bodyA, "bodyA", entity, report);
            joint.bodyB = require_uuid(scene, joint.bodyB, "bodyB", entity, report);
        }
        if (entity.Has<ColliderComponent>())
        {
            ColliderComponent& collider = entity.Get<ColliderComponent>();
            collider.physicsMaterial = require_uuid(scene, collider.physicsMaterial, "physics material", entity, report);
        }
        if (entity.Has<RobotComponent>())
        {
            RobotComponent& robot = entity.Get<RobotComponent>();
            robot.rootLink = require_uuid(scene, robot.rootLink, "root link", entity, report);
        }
        if (entity.Has<LinkComponent>())
        {
            LinkComponent& link = entity.Get<LinkComponent>();
            link.robot = require_uuid(scene, link.robot, "robot", entity, report);
        }
        if (entity.Has<ActuatorComponent>())
        {
            ActuatorComponent& actuator = entity.Get<ActuatorComponent>();
            actuator.joint = require_uuid(scene, actuator.joint, "joint", entity, report);
        }
        if (entity.Has<SensorComponent>())
        {
            SensorComponent& sensor = entity.Get<SensorComponent>();
            sensor.link = require_uuid(scene, sensor.link, "link", entity, report);
        }
    }

    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity || !entity.Has<RelationshipComponent>())
            continue;
        std::unordered_set<UUID> seen;
        Entity cursor = entity;
        while (cursor && cursor.Has<RelationshipComponent>())
        {
            const UUID current = cursor.GetUUID();
            if (!seen.insert(current).second)
            {
                report.warning("Hierarchy cycle at " + cursor.Get<TagComponent>().name + " was broken.");
                clear_parent(scene, cursor);
                break;
            }
            const UUID parent = cursor.Get<RelationshipComponent>().parent;
            if (parent == kNullUuid)
                break;
            cursor = scene.FindByUUID(parent);
        }
    }
}

bool finite(float value)
{
    return std::isfinite(value);
}

bool finite(const Vec3& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool finite(const Quat& value)
{
    return finite(value.x) && finite(value.y) && finite(value.z) && finite(value.w);
}

bool finite(const Rgb& value)
{
    return finite(value.r) && finite(value.g) && finite(value.b);
}

bool usable_quat(const Quat& value)
{
    if (!finite(value))
        return false;
    const float length2 = value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;
    return length2 > 1e-12f;
}

void check_finite(float& value, float fallback, const std::string& name, const char* field, LoadReport& report)
{
    if (finite(value))
        return;
    value = fallback;
    report.warning(name + " " + field + " was not finite and was reset.");
}

void check_finite(Vec3& value, const Vec3& fallback, const std::string& name, const char* field, LoadReport& report)
{
    if (finite(value))
        return;
    value = fallback;
    report.warning(name + " " + field + " was not finite and was reset.");
}

void check_finite(Rgb& value, const Rgb& fallback, const std::string& name, const char* field, LoadReport& report)
{
    if (finite(value))
        return;
    value = fallback;
    report.warning(name + " " + field + " was not finite and was reset.");
}

void check_quat(Quat& value, const std::string& name, const char* field, LoadReport& report)
{
    if (usable_quat(value))
        return;
    value = Quat{};
    report.warning(name + " " + field + " was not a usable rotation and was reset.");
}

void check_size(Vec3& value, const Vec3& fallback, const std::string& name, const char* field, LoadReport& report)
{
    bool bad = false;
    for (int axis = 0; axis < 3; ++axis)
    {
        float& component = (&value.x)[axis];
        if (finite(component) && component > 0.0f)
            continue;
        component = (&fallback.x)[axis];
        bad = true;
    }
    if (bad)
        report.warning(name + " " + field + " had a non-positive size and was reset.");
}

void check_positive(float& value, float fallback, const std::string& name, const char* field, LoadReport& report)
{
    if (finite(value) && value > 0.0f)
        return;
    value = fallback;
    report.warning(name + " " + field + " was not positive and was reset.");
}

void check_axis(Vec3& value, const std::string& name, const char* field, LoadReport& report)
{
    if (finite(value) && (value.x != 0.0f || value.y != 0.0f || value.z != 0.0f))
        return;
    value = Vec3{0.0f, 1.0f, 0.0f};
    report.warning(name + " " + field + " had no direction and was reset.");
}

// Loaded numbers are untrusted. Anything non-finite or physically impossible is
// replaced with the component default and noted in the load report. Mass is only
// forced positive for dynamic bodies: a static or kinematic body may carry a zero
// mass from an exporter and must keep it so re-saving stays byte identical.
void validate_scene(Scene& scene, LoadReport& report)
{
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity)
            continue;
        const std::string name = entity.Has<TagComponent>() ? entity.Get<TagComponent>().name : std::string("Entity");

        if (entity.Has<TransformComponent>())
        {
            TransformComponent& transform = entity.Get<TransformComponent>();
            check_finite(transform.position, Vec3{}, name, "position", report);
            check_finite(transform.scale, Vec3{1.0f, 1.0f, 1.0f}, name, "scale", report);
            check_quat(transform.rotation, name, "rotation", report);
        }
        if (entity.Has<PrimitiveBoxComponent>())
        {
            PrimitiveBoxComponent& box = entity.Get<PrimitiveBoxComponent>();
            check_size(box.size, Vec3{2.0f, 2.0f, 2.0f}, name, "box size", report);
            check_finite(box.color, Rgb{0.15f, 0.35f, 0.85f}, name, "box color", report);
        }
        if (entity.Has<MaterialOverrideComponent>())
        {
            MaterialOverrideComponent& material = entity.Get<MaterialOverrideComponent>();
            check_finite(material.albedo, Rgb{1.0f, 1.0f, 1.0f}, name, "albedo", report);
            check_finite(material.roughness, 0.5f, name, "roughness", report);
            check_finite(material.metallic, 0.0f, name, "metallic", report);
        }
        if (entity.Has<RigidBodyComponent>())
        {
            RigidBodyComponent& body = entity.Get<RigidBodyComponent>();
            if (!finite(body.mass) || (body.type == BodyType::Dynamic && body.mass <= 0.0f))
            {
                body.mass = 1.0f;
                report.warning(name + " mass was not a positive value and was reset to 1.");
            }
            bool inertia_ok = true;
            for (const float entry : body.inertia.m)
                inertia_ok = inertia_ok && finite(entry);
            if (!inertia_ok)
            {
                body.inertia = Mat3{};
                report.warning(name + " inertia was not finite and was reset to identity.");
            }
            check_finite(body.linearVelocity, Vec3{}, name, "linear velocity", report);
            check_finite(body.angularVelocity, Vec3{}, name, "angular velocity", report);
            check_finite(body.linearDamping, 0.0f, name, "linear damping", report);
            check_finite(body.angularDamping, 0.05f, name, "angular damping", report);
        }
        if (entity.Has<ColliderComponent>())
        {
            ColliderComponent& collider = entity.Get<ColliderComponent>();
            if (collider.shape == ShapeType::Box)
                check_size(collider.halfExtents, Vec3{1.0f, 1.0f, 1.0f}, name, "collider half extents", report);
            if (collider.shape == ShapeType::Sphere || collider.shape == ShapeType::Capsule ||
                collider.shape == ShapeType::Cylinder)
                check_positive(collider.radius, 0.5f, name, "collider radius", report);
            if (collider.shape == ShapeType::Capsule || collider.shape == ShapeType::Cylinder)
                check_positive(collider.height, 1.0f, name, "collider height", report);
            check_finite(collider.offset, Vec3{}, name, "collider offset", report);
            check_quat(collider.rotation, name, "collider rotation", report);
        }
        if (entity.Has<PhysicsMaterialComponent>())
        {
            PhysicsMaterialComponent& material = entity.Get<PhysicsMaterialComponent>();
            check_finite(material.friction, 0.5f, name, "friction", report);
            check_finite(material.restitution, 0.0f, name, "restitution", report);
            check_finite(material.rollingFriction, 0.0f, name, "rolling friction", report);
        }
        if (entity.Has<JointComponent>())
        {
            JointComponent& joint = entity.Get<JointComponent>();
            check_finite(joint.anchorA, Vec3{}, name, "joint anchor A", report);
            check_finite(joint.anchorB, Vec3{}, name, "joint anchor B", report);
            check_axis(joint.axisA, name, "joint axis A", report);
            check_axis(joint.axisB, name, "joint axis B", report);
            check_finite(joint.limitMin, 0.0f, name, "joint limit min", report);
            check_finite(joint.limitMax, 0.0f, name, "joint limit max", report);
            check_finite(joint.motorTarget, 0.0f, name, "joint motor target", report);
            check_finite(joint.motorMaxForce, 0.0f, name, "joint motor force", report);
        }
        if (entity.Has<CfdDomainComponent>())
        {
            CfdDomainComponent& domain = entity.Get<CfdDomainComponent>();
            check_finite(domain.boundsMin, Vec3{-1.0f, -1.0f, -1.0f}, name, "CFD bounds min", report);
            check_finite(domain.boundsMax, Vec3{1.0f, 1.0f, 1.0f}, name, "CFD bounds max", report);
            if (domain.resolutionX <= 0 || domain.resolutionY <= 0 || domain.resolutionZ <= 0)
            {
                if (domain.resolutionX <= 0)
                    domain.resolutionX = 32;
                if (domain.resolutionY <= 0)
                    domain.resolutionY = 32;
                if (domain.resolutionZ <= 0)
                    domain.resolutionZ = 32;
                report.warning(name + " CFD resolution was not positive and was reset.");
            }
            check_positive(domain.density, 1.2f, name, "CFD density", report);
            check_positive(domain.viscosity, 1.8e-5f, name, "CFD viscosity", report);
            for (BoundaryCondition& boundary : domain.boundaries)
            {
                check_finite(boundary.velocity, Vec3{}, name, "boundary velocity", report);
                check_finite(boundary.pressure, 0.0f, name, "boundary pressure", report);
            }
        }
    }
}

} // namespace

nlohmann::json WriteScene(const Scene& scene)
{
    EnsureComponentsRegistered();
    nlohmann::json entities = nlohmann::json::array();
    const ComponentRegistry& registry = ComponentRegistry::Instance();

    for (const UUID id : scene.CreationOrder())
    {
        const Entity entity = scene.FindByUUID(id);
        if (!entity)
            continue;

        nlohmann::json components = nlohmann::json::object();
        for (const ComponentInfo& info : registry.All())
        {
            if (!info.serializable || info.name == "ID" || !info.has(entity))
                continue;
            components[info.name] = info.serialize(entity);
        }

        const auto unknown = scene.UnknownComponents().find(id);
        if (unknown != scene.UnknownComponents().end())
        {
            for (const Scene::UnknownComponent& extra : unknown->second)
            {
                try
                {
                    components[extra.name] = nlohmann::json::parse(extra.json);
                }
                catch (const nlohmann::json::exception&)
                {
                    components[extra.name] = extra.json;
                }
            }
        }

        entities.push_back(nlohmann::json{{"id", UuidToHex(id)}, {"components", std::move(components)}});
    }

    return nlohmann::json{{"entities", std::move(entities)}};
}

std::expected<void, LoadError> ReadScene(const nlohmann::json& json, Scene& scene, LoadReport& report)
{
    if (!json.is_object() || !json.contains("entities") || !json.at("entities").is_array())
        return std::unexpected(LoadError{LoadError::Kind::Schema, "Scene is missing an entities array."});
    if (!depth_ok(json, 0))
        return std::unexpected(LoadError{LoadError::Kind::Schema, "Scene JSON is nested too deeply."});

    EnsureComponentsRegistered();
    Scene loaded;
    const ComponentRegistry& registry = ComponentRegistry::Instance();

    try
    {
        for (const nlohmann::json& item : json.at("entities"))
        {
            if (!item.is_object() || !item.contains("id") || !item.at("id").is_string())
            {
                report.warning("Skipped an entity with no id.");
                continue;
            }
            const std::optional<UUID> id = UuidFromHex(item.at("id").get<std::string>());
            if (!id || *id == kNullUuid)
            {
                report.warning("Skipped an entity with an invalid id.");
                continue;
            }
            if (loaded.FindByUUID(*id))
            {
                report.warning("Skipped a duplicate entity id " + item.at("id").get<std::string>() + ".");
                continue;
            }

            std::string name = "Entity";
            const nlohmann::json components = item.value("components", nlohmann::json::object());
            if (!components.is_object())
            {
                report.warning("Skipped entity " + item.at("id").get<std::string>() + " because components is not an object.");
                continue;
            }
            if (components.contains("Tag") && components.at("Tag").is_object())
                name = components.at("Tag").value("name", std::string("Entity"));

            Entity entity = loaded.CreateEntityWithUUID(*id, name);
            if (!entity)
            {
                report.warning("Could not create entity " + item.at("id").get<std::string>() + ".");
                continue;
            }

            std::vector<Scene::UnknownComponent> unknown;
            for (const auto& field : components.items())
            {
                if (field.key() == "ID")
                    continue;
                const ComponentInfo* info = registry.Find(field.key());
                if (info == nullptr || !info->serializable)
                {
                    report.warning("Unknown component \"" + field.key() + "\" was kept.");
                    unknown.push_back(Scene::UnknownComponent{field.key(), field.value().dump()});
                    continue;
                }
                info->deserialize(entity, field.value(), report);
            }
            loaded.SetUnknownComponents(*id, std::move(unknown));
        }
    }
    catch (const nlohmann::json::exception& ex)
    {
        return std::unexpected(LoadError{LoadError::Kind::Corrupt, std::string("Scene JSON could not be read: ") + ex.what()});
    }

    resolve_references(loaded, report);
    validate_scene(loaded, report);
    scene = std::move(loaded);
    return {};
}

} // namespace openphysx
