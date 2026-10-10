#pragma once

#include "ecs/components/core.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/entity.hpp"

#include <entt/entt.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace openphysx {

// Layout version of the scene. Bump it when a slot changes shape in a way older builds
// cannot read. A file with a newer version is refused, never silently rewritten.
inline constexpr std::uint32_t kSceneVersion = 1;

struct SceneBackref
{
    class Scene* scene = nullptr;
};

// One registry. Other code reaches it through Scene&.
class Scene
{
public:
    Scene();
    Scene(const Scene& other);
    Scene& operator=(const Scene& other);
    Scene(Scene&& other) noexcept;
    Scene& operator=(Scene&& other) noexcept;
    ~Scene() = default;

    Entity CreateEntity(std::string_view name = "Entity");
    Entity CreateEntityWithUUID(UUID id, std::string_view name);
    void DestroyEntity(Entity entity);
    void FlushDestroyed();

    Entity FindByUUID(UUID id) const;
    Entity FindByName(std::string_view name) const;

    // A name no other entity in this scene uses: "Cube", "Cube.001", "Cube.002", ...
    // `ignore` is the entity being renamed, so it does not clash with itself.
    std::string UniqueName(std::string_view requested, UUID ignore = kNullUuid) const;
    // Renames the entity to UniqueName(requested) and returns the name it received.
    std::string SetName(Entity entity, std::string_view requested);

    template<typename... Components>
    auto View()
    {
        return registry_.view<Components...>();
    }

    template<typename... Components>
    auto View() const
    {
        return registry_.view<Components...>();
    }

    entt::registry& Registry() { return registry_; }
    const entt::registry& Registry() const { return registry_; }

    // Rejects a parent that is the child or one of its descendants.
    // Destroying a parent destroys its children on the next flush.
    bool SetParent(Entity child, Entity parent, bool keep_world_transform);
    TransformComponent GetWorldTransform(Entity entity) const;
    // Stores a world transform as the local transform, relative to the parent.
    void SetWorldTransform(Entity entity, const TransformComponent& world) { ApplyWorld(entity, world); }

    const std::vector<UUID>& CreationOrder() const { return creation_order_; }
    std::uint32_t Version() const { return version_; }

    // Components from a file that this build does not know. They are written back unchanged.
    struct UnknownComponent
    {
        std::string name;
        std::string json;
    };

    void SetUnknownComponents(UUID id, std::vector<UnknownComponent> components);
    const std::unordered_map<UUID, std::vector<UnknownComponent>>& UnknownComponents() const { return unknown_; }

    friend bool operator==(const Scene& a, const Scene& b);

private:
    static void OnIdConstruct(entt::registry& registry, entt::entity entity);
    static void OnIdDestroy(entt::registry& registry, entt::entity entity);

    void CopyEntitiesFrom(const Scene& other);
    void Detach(Entity entity);
    bool IsUnder(Entity ancestor, Entity node) const;
    void ApplyWorld(Entity entity, const TransformComponent& world);
    bool NameTaken(std::string_view name, UUID ignore) const;
    void Rebind();

    entt::registry registry_;
    std::unordered_map<UUID, entt::entity> uuid_map_;
    std::vector<UUID> creation_order_;
    std::vector<UUID> destroy_queue_;
    std::unordered_set<UUID> pending_;
    std::unordered_map<UUID, std::vector<UnknownComponent>> unknown_;
    std::uint32_t version_ = kSceneVersion;
};

inline bool operator==(const Scene::UnknownComponent& a, const Scene::UnknownComponent& b)
{
    return a.name == b.name && a.json == b.json;
}

bool operator==(const Scene& a, const Scene& b);

// Adding a component the entity already has replaces its value. EnTT leaves
// emplacing over an existing component undefined, so it is never called here.
template<typename T, typename... Args>
T& Entity::Add(Args&&... args)
{
    if (Has<T>())
    {
        T& current = scene_->Registry().get<T>(id_);
        current = T(std::forward<Args>(args)...);
        return current;
    }
    scene_->Registry().emplace<T>(id_, std::forward<Args>(args)...);
    return scene_->Registry().get<T>(id_);
}

template<typename T>
T& Entity::Get()
{
    return scene_->Registry().get<T>(id_);
}

template<typename T>
const T& Entity::Get() const
{
    return scene_->Registry().get<T>(id_);
}

template<typename T>
bool Entity::Has() const
{
    return scene_ != nullptr && scene_->Registry().valid(id_) && scene_->Registry().all_of<T>(id_);
}

template<typename T>
void Entity::Remove()
{
    if (Has<T>())
        scene_->Registry().remove<T>(id_);
}

} // namespace openphysx
