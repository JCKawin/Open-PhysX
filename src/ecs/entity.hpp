#pragma once

#include "ecs/uuid.hpp"

#include <entt/entity/entity.hpp>

namespace openphysx {

class Scene;

// Thin handle. It is not saved and must not be stored past the frame that spawned it.
// Stable identity is the UUID.
class Entity
{
public:
    Entity() = default;
    Entity(entt::entity id, Scene* scene);

    template<typename T, typename... Args>
    T& Add(Args&&... args);

    template<typename T>
    T& Get();

    template<typename T>
    const T& Get() const;

    template<typename T>
    bool Has() const;

    template<typename T>
    void Remove();

    UUID GetUUID() const;
    explicit operator bool() const;

    entt::entity Raw() const { return id_; }

private:
    entt::entity id_ = entt::null;
    Scene* scene_ = nullptr;
};

} // namespace openphysx
