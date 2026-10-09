#pragma once

#include "core/Types.h"
#include "ecs/components/core.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/scene.hpp"

namespace openphysx {

inline RigidBody EntityPose(const Entity& entity)
{
    RigidBody body;
    if (!entity)
        return body;
    if (entity.Has<TransformComponent>())
    {
        const TransformComponent& transform = entity.Get<TransformComponent>();
        body.position = transform.position;
        body.rotation = transform.rotation;
    }
    if (entity.Has<PrimitiveBoxComponent>())
    {
        const PrimitiveBoxComponent& box = entity.Get<PrimitiveBoxComponent>();
        body.size = box.size;
        body.color = box.color;
    }
    return body;
}

inline void SetEntityPose(Entity entity, const RigidBody& body)
{
    if (!entity)
        return;
    if (entity.Has<TransformComponent>())
    {
        TransformComponent& transform = entity.Get<TransformComponent>();
        transform.position = body.position;
        transform.rotation = body.rotation;
    }
    if (entity.Has<PrimitiveBoxComponent>())
    {
        PrimitiveBoxComponent& box = entity.Get<PrimitiveBoxComponent>();
        box.size = body.size;
        box.color = body.color;
    }
}

inline bool EntityVisible(const Entity& entity)
{
    return entity && (!entity.Has<EditorStateComponent>() || entity.Get<EditorStateComponent>().visible);
}

inline bool EntitySelected(const Entity& entity)
{
    return entity && entity.Has<SelectionOutlineTag>();
}

inline void SetEntitySelected(Entity entity, bool selected)
{
    if (!entity)
        return;
    if (selected)
    {
        if (!entity.Has<SelectionOutlineTag>())
            entity.Add<SelectionOutlineTag>();
    }
    else if (entity.Has<SelectionOutlineTag>())
    {
        entity.Remove<SelectionOutlineTag>();
    }
}

} // namespace openphysx
