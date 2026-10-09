#include "persistence/scene_serializer.hpp"

#include "ecs/component_registry.hpp"
#include "ecs/components/physics.hpp"
#include "ecs/components/robot.hpp"
#include "ecs/components/serialize.hpp"

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
    scene = std::move(loaded);
    return {};
}

} // namespace openphysx
