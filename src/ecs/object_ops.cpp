#include "ecs/object_ops.hpp"

#include "ecs/component_registry.hpp"
#include "ecs/components/core.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/pose.hpp"

#include <unordered_map>
#include <unordered_set>

namespace openphysx {
namespace {

constexpr int kMaxDepth = 64;

UUID parent_of(const Entity& entity)
{
    return entity && entity.Has<RelationshipComponent>() ? entity.Get<RelationshipComponent>().parent : kNullUuid;
}

bool ancestor_selected(const Scene& scene, const Entity& entity)
{
    UUID parent = parent_of(entity);
    for (int depth = 0; parent != kNullUuid && depth < kMaxDepth; ++depth)
    {
        const Entity ancestor = scene.FindByUUID(parent);
        if (!ancestor)
            return false;
        if (EntitySelected(ancestor))
            return true;
        parent = parent_of(ancestor);
    }
    return false;
}

} // namespace

bool IsVisibleInWorld(const Scene& scene, const Entity& entity)
{
    if (!entity)
        return false;
    Entity current = entity;
    for (int depth = 0; depth < kMaxDepth; ++depth)
    {
        if (!EntityVisible(current))
            return false;
        const UUID parent = parent_of(current);
        if (parent == kNullUuid)
            return true;
        const Entity next = scene.FindByUUID(parent);
        if (!next)
            return true;
        current = next;
    }
    return true;
}

std::vector<UUID> DrawableObjects(const Scene& scene)
{
    std::vector<UUID> ids;
    for (const UUID id : scene.CreationOrder())
    {
        const Entity entity = scene.FindByUUID(id);
        if (entity && entity.Has<PrimitiveBoxComponent>() && IsVisibleInWorld(scene, entity))
            ids.push_back(id);
    }
    return ids;
}

std::vector<UUID> SelectedObjects(const Scene& scene)
{
    std::vector<UUID> ids;
    for (const UUID id : scene.CreationOrder())
    {
        if (EntitySelected(scene.FindByUUID(id)))
            ids.push_back(id);
    }
    return ids;
}

std::vector<UUID> TransformRoots(const Scene& scene)
{
    std::vector<UUID> ids;
    for (const UUID id : scene.CreationOrder())
    {
        const Entity entity = scene.FindByUUID(id);
        if (!EntitySelected(entity) || !IsVisibleInWorld(scene, entity) || ancestor_selected(scene, entity))
            continue;
        ids.push_back(id);
    }
    return ids;
}

void SetSelected(Scene& scene, UUID id, bool selected)
{
    SetEntitySelected(scene.FindByUUID(id), selected);
}

void DeselectAll(Scene& scene)
{
    for (const UUID id : scene.CreationOrder())
        SetEntitySelected(scene.FindByUUID(id), false);
}

void SelectAllVisible(Scene& scene)
{
    for (const UUID id : scene.CreationOrder())
    {
        const Entity entity = scene.FindByUUID(id);
        SetEntitySelected(entity, IsVisibleInWorld(scene, entity));
    }
}

UUID ResolveActive(const Scene& scene, UUID active)
{
    if (active != kNullUuid && EntitySelected(scene.FindByUUID(active)))
        return active;
    UUID last = kNullUuid;
    for (const UUID id : scene.CreationOrder())
    {
        if (EntitySelected(scene.FindByUUID(id)))
            last = id;
    }
    return last;
}

void HideSelected(Scene& scene)
{
    for (const UUID id : SelectedObjects(scene))
    {
        Entity entity = scene.FindByUUID(id);
        if (entity.Has<EditorStateComponent>())
            entity.Get<EditorStateComponent>().visible = false;
    }
}

void RevealHidden(Scene& scene)
{
    for (const UUID id : scene.CreationOrder())
    {
        Entity entity = scene.FindByUUID(id);
        if (!entity.Has<EditorStateComponent>() || entity.Get<EditorStateComponent>().visible)
            continue;
        entity.Get<EditorStateComponent>().visible = true;
        SetEntitySelected(entity, true);
    }
}

void DeleteObjects(Scene& scene, const std::vector<UUID>& ids)
{
    const std::unordered_set<UUID> doomed(ids.begin(), ids.end());
    for (const UUID id : ids)
    {
        const Entity entity = scene.FindByUUID(id);
        if (!entity || !entity.Has<RelationshipComponent>())
            continue;
        const std::vector<UUID> children = entity.Get<RelationshipComponent>().children;
        for (const UUID child : children)
        {
            if (!doomed.contains(child))
                scene.SetParent(scene.FindByUUID(child), Entity{}, true);
        }
    }

    for (const UUID id : ids)
        scene.DestroyEntity(scene.FindByUUID(id));
    scene.FlushDestroyed();
}

std::vector<UUID> DuplicateObjects(Scene& scene, const std::vector<UUID>& ids)
{
    EnsureComponentsRegistered();
    const std::unordered_set<UUID> chosen(ids.begin(), ids.end());
    std::vector<UUID> sources;
    for (const UUID id : scene.CreationOrder())
    {
        if (chosen.contains(id))
            sources.push_back(id);
    }

    // Name, id, and the hierarchy links are set here. Every other saved component is copied
    // by value through the registry's copy hook.
    std::unordered_map<UUID, UUID> copy_of;
    std::vector<UUID> copies;
    for (const UUID id : sources)
    {
        const Entity source = scene.FindByUUID(id);
        Entity copy = scene.CreateEntity(source.Get<TagComponent>().name);
        for (const ComponentInfo& info : ComponentRegistry::Instance().All())
        {
            if (!info.replicate || !info.copy)
                continue;
            if (info.name == "ID" || info.name == "Tag" || info.name == "Relationship")
                continue;
            info.copy(source, copy);
        }
        copy_of[id] = copy.GetUUID();
        copies.push_back(copy.GetUUID());
    }

    for (const UUID id : sources)
    {
        UUID parent = scene.FindByUUID(id).Get<RelationshipComponent>().parent;
        if (const auto mapped = copy_of.find(parent); mapped != copy_of.end())
            parent = mapped->second;
        if (parent != kNullUuid)
            scene.SetParent(scene.FindByUUID(copy_of[id]), scene.FindByUUID(parent), false);
    }

    for (const UUID id : sources)
        SetSelected(scene, id, false);
    for (const UUID id : copies)
        SetSelected(scene, id, true);
    return copies;
}

UUID AddBoxObject(Scene& scene)
{
    DeselectAll(scene);
    // Each new box steps 2.5 m along X, so it does not hide the boxes already in the scene.
    const float x = 2.5f * static_cast<float>(scene.CreationOrder().size());
    Entity entity = scene.CreateEntity("Cube");
    entity.Get<TransformComponent>().position = {x, 1.0f, 0.0f};
    entity.Add<PrimitiveBoxComponent>();
    SetEntitySelected(entity, true);
    return entity.GetUUID();
}

} // namespace openphysx
