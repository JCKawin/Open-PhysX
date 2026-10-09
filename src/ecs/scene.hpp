#pragma once

#include "ecs/components/core.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/entity.hpp"

#include <entt/entt.hpp>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace openphysx {

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

    const std::vector<UUID>& CreationOrder() const { return creation_order_; }

    friend bool operator==(const Scene& a, const Scene& b);

private:
    static void OnIdConstruct(entt::registry& registry, entt::entity entity);
    static void OnIdDestroy(entt::registry& registry, entt::entity entity);

    void CopyEntitiesFrom(const Scene& other);
    void Detach(Entity entity);
    bool IsUnder(Entity ancestor, Entity node) const;
    void ApplyWorld(Entity entity, const TransformComponent& world);
    void Rebind();

    entt::registry registry_;
    std::unordered_map<UUID, entt::entity> uuid_map_;
    std::vector<UUID> creation_order_;
    std::vector<UUID> destroy_queue_;
    std::unordered_set<UUID> pending_;
};

bool operator==(const Scene& a, const Scene& b);

template<typename T, typename... Args>
T& Entity::Add(Args&&... args)
{
    // EnTT's emplace returns void for empty tags, so the reference always comes from get.
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
