#include "ecs/scene.hpp"

#include "core/Math.h"
#include "ecs/component_registry.hpp"

#include <algorithm>
#include <cmath>

namespace openphysx {
namespace {

Quat quat_conjugate(Quat q)
{
    return {-q.x, -q.y, -q.z, q.w};
}

float safe_div(float value, float scale)
{
    if (std::fabs(scale) <= 1.0e-8f)
        return value;
    return value / scale;
}

} // namespace

Entity::Entity(entt::entity id, Scene* scene)
    : id_(id)
    , scene_(scene)
{
}

UUID Entity::GetUUID() const
{
    if (!Has<IDComponent>())
        return kNullUuid;
    return Get<IDComponent>().id;
}

Entity::operator bool() const
{
    return scene_ != nullptr && id_ != entt::null && scene_->Registry().valid(id_);
}

Scene::Scene()
{
    EnsureComponentsRegistered();
    registry_.ctx().emplace<SceneBackref>(SceneBackref{this});
    registry_.on_construct<IDComponent>().connect<&Scene::OnIdConstruct>();
    registry_.on_destroy<IDComponent>().connect<&Scene::OnIdDestroy>();
}

Scene::Scene(const Scene& other)
    : Scene()
{
    CopyEntitiesFrom(other);
}

Scene& Scene::operator=(const Scene& other)
{
    if (this == &other)
        return *this;
    registry_.clear();
    uuid_map_.clear();
    creation_order_.clear();
    destroy_queue_.clear();
    pending_.clear();
    CopyEntitiesFrom(other);
    return *this;
}

Scene::Scene(Scene&& other) noexcept
    : registry_(std::move(other.registry_))
    , uuid_map_(std::move(other.uuid_map_))
    , creation_order_(std::move(other.creation_order_))
    , destroy_queue_(std::move(other.destroy_queue_))
    , pending_(std::move(other.pending_))
{
    Rebind();
}

Scene& Scene::operator=(Scene&& other) noexcept
{
    if (this == &other)
        return *this;
    registry_ = std::move(other.registry_);
    uuid_map_ = std::move(other.uuid_map_);
    creation_order_ = std::move(other.creation_order_);
    destroy_queue_ = std::move(other.destroy_queue_);
    pending_ = std::move(other.pending_);
    Rebind();
    return *this;
}

void Scene::Rebind()
{
    if (auto* back = registry_.ctx().find<SceneBackref>())
        back->scene = this;
}

void Scene::OnIdConstruct(entt::registry& registry, entt::entity entity)
{
    Scene* scene = registry.ctx().get<SceneBackref>().scene;
    if (scene == nullptr)
        return;
    const UUID id = registry.get<IDComponent>(entity).id;
    scene->uuid_map_[id] = entity;
    if (std::find(scene->creation_order_.begin(), scene->creation_order_.end(), id) == scene->creation_order_.end())
        scene->creation_order_.push_back(id);
}

void Scene::OnIdDestroy(entt::registry& registry, entt::entity entity)
{
    Scene* scene = registry.ctx().get<SceneBackref>().scene;
    if (scene == nullptr)
        return;
    const UUID id = registry.get<IDComponent>(entity).id;
    scene->uuid_map_.erase(id);
    std::erase(scene->creation_order_, id);
}

Entity Scene::CreateEntity(std::string_view name)
{
    return CreateEntityWithUUID(GenerateUuid(), name);
}

Entity Scene::CreateEntityWithUUID(UUID id, std::string_view name)
{
    if (id == kNullUuid || uuid_map_.contains(id))
        return {};

    const entt::entity created = registry_.create();
    registry_.emplace<IDComponent>(created, IDComponent{id});
    registry_.emplace<TagComponent>(created, TagComponent{std::string(name)});
    registry_.emplace<TransformComponent>(created);
    registry_.emplace<RelationshipComponent>(created);
    registry_.emplace<EditorStateComponent>(created);
    return Entity{created, this};
}

void Scene::DestroyEntity(Entity entity)
{
    if (!entity || !entity.Has<IDComponent>())
        return;

    std::vector<UUID> stack;
    stack.push_back(entity.GetUUID());
    while (!stack.empty())
    {
        const UUID id = stack.back();
        stack.pop_back();
        if (!pending_.insert(id).second)
            continue;
        destroy_queue_.push_back(id);
        const Entity current = FindByUUID(id);
        if (!current || !current.Has<RelationshipComponent>())
            continue;
        const std::vector<UUID> children = current.Get<RelationshipComponent>().children;
        for (const UUID child : children)
            stack.push_back(child);
    }
}

void Scene::FlushDestroyed()
{
    std::vector<UUID> queue;
    queue.swap(destroy_queue_);
    pending_.clear();
    for (const UUID id : queue)
    {
        const Entity entity = FindByUUID(id);
        if (!entity)
            continue;
        Detach(entity);
        registry_.destroy(entity.Raw());
    }
}

Entity Scene::FindByUUID(UUID id) const
{
    const auto it = uuid_map_.find(id);
    if (it == uuid_map_.end())
        return {};
    return Entity{it->second, const_cast<Scene*>(this)};
}

Entity Scene::FindByName(std::string_view name) const
{
    for (const UUID id : creation_order_)
    {
        const Entity entity = FindByUUID(id);
        if (entity && entity.Has<TagComponent>() && entity.Get<TagComponent>().name == name)
            return entity;
    }
    return {};
}

void Scene::Detach(Entity entity)
{
    if (!entity || !entity.Has<RelationshipComponent>())
        return;
    RelationshipComponent& link = entity.Get<RelationshipComponent>();
    if (link.parent != kNullUuid)
    {
        Entity parent = FindByUUID(link.parent);
        if (parent && parent.Has<RelationshipComponent>())
        {
            std::vector<UUID>& children = parent.Get<RelationshipComponent>().children;
            std::erase(children, entity.GetUUID());
        }
    }
    link.parent = kNullUuid;
}

bool Scene::IsUnder(Entity ancestor, Entity node) const
{
    if (!ancestor || !node)
        return false;
    UUID id = node.GetUUID();
    const UUID stop = ancestor.GetUUID();
    while (id != kNullUuid)
    {
        if (id == stop)
            return true;
        const Entity current = FindByUUID(id);
        if (!current || !current.Has<RelationshipComponent>())
            return false;
        id = current.Get<RelationshipComponent>().parent;
    }
    return false;
}

TransformComponent Scene::GetWorldTransform(Entity entity) const
{
    TransformComponent local{};
    if (!entity || !entity.Has<TransformComponent>())
        return local;
    local = entity.Get<TransformComponent>();
    if (!entity.Has<RelationshipComponent>())
        return local;

    const UUID parent_id = entity.Get<RelationshipComponent>().parent;
    if (parent_id == kNullUuid)
        return local;
    const Entity parent = FindByUUID(parent_id);
    if (!parent)
        return local;

    const TransformComponent world_parent = GetWorldTransform(parent);
    TransformComponent world;
    world.scale = {
        world_parent.scale.x * local.scale.x,
        world_parent.scale.y * local.scale.y,
        world_parent.scale.z * local.scale.z,
    };
    world.rotation = quat_normalize(quat_mul(world_parent.rotation, local.rotation));
    const Vec3 scaled{
        local.position.x * world_parent.scale.x,
        local.position.y * world_parent.scale.y,
        local.position.z * world_parent.scale.z,
    };
    world.position = vec_add(world_parent.position, quat_rotate(world_parent.rotation, scaled));
    return world;
}

void Scene::ApplyWorld(Entity entity, const TransformComponent& world)
{
    if (!entity || !entity.Has<TransformComponent>())
        return;
    TransformComponent& local = entity.Get<TransformComponent>();
    const UUID parent_id = entity.Has<RelationshipComponent>() ? entity.Get<RelationshipComponent>().parent : kNullUuid;
    if (parent_id == kNullUuid)
    {
        local = world;
        return;
    }
    const Entity parent = FindByUUID(parent_id);
    if (!parent)
    {
        local = world;
        return;
    }
    const TransformComponent world_parent = GetWorldTransform(parent);
    local.scale = {
        safe_div(world.scale.x, world_parent.scale.x),
        safe_div(world.scale.y, world_parent.scale.y),
        safe_div(world.scale.z, world_parent.scale.z),
    };
    local.rotation = quat_normalize(quat_mul(quat_conjugate(world_parent.rotation), world.rotation));
    const Vec3 relative = vec_sub(world.position, world_parent.position);
    const Vec3 unrotated = quat_rotate(quat_conjugate(world_parent.rotation), relative);
    local.position = {
        safe_div(unrotated.x, world_parent.scale.x),
        safe_div(unrotated.y, world_parent.scale.y),
        safe_div(unrotated.z, world_parent.scale.z),
    };
}

bool Scene::SetParent(Entity child, Entity parent, bool keep_world_transform)
{
    if (!child || !child.Has<RelationshipComponent>() || !child.Has<TransformComponent>())
        return false;
    if (parent && (!parent.Has<RelationshipComponent>() || parent.Raw() == child.Raw()))
        return false;
    if (parent && IsUnder(child, parent))
        return false;

    const TransformComponent world = GetWorldTransform(child);
    Detach(child);
    if (parent)
    {
        child.Get<RelationshipComponent>().parent = parent.GetUUID();
        parent.Get<RelationshipComponent>().children.push_back(child.GetUUID());
    }
    if (keep_world_transform)
        ApplyWorld(child, world);
    return true;
}

void Scene::CopyEntitiesFrom(const Scene& other)
{
    for (const UUID id : other.creation_order_)
    {
        const Entity source = other.FindByUUID(id);
        if (!source || !source.Has<TagComponent>())
            continue;
        Entity copy = CreateEntityWithUUID(id, source.Get<TagComponent>().name);
        if (!copy)
            continue;
        for (const ComponentInfo& info : ComponentRegistry::Instance().All())
        {
            if (info.replicate && info.copy)
                info.copy(source, copy);
        }
    }
}

bool operator==(const Scene& a, const Scene& b)
{
    EnsureComponentsRegistered();
    if (a.creation_order_ != b.creation_order_)
        return false;
    for (const UUID id : a.creation_order_)
    {
        const Entity left = a.FindByUUID(id);
        const Entity right = b.FindByUUID(id);
        if (!left || !right)
            return false;
        for (const ComponentInfo& info : ComponentRegistry::Instance().All())
        {
            if (!info.compares || !info.equals)
                continue;
            if (!info.equals(left, right))
                return false;
        }
    }
    return true;
}

} // namespace openphysx
