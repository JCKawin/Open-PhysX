#pragma once

#include "core/Types.h"
#include "ecs/components/core.hpp"
#include "ecs/components/primitive.hpp"
#include "ecs/components/runtime.hpp"
#include "ecs/scene.hpp"

namespace openphysx {

// Pose used for drawing and picking: the world transform, with the box scaled by the
// world scale. The box size stays the box, and scale stays on the transform.
inline RigidBody WorldBody(const Scene& scene, const Entity& entity)
{
    RigidBody body;
    if (!entity)
        return body;
    const TransformComponent world = scene.GetWorldTransform(entity);
    body.position = world.position;
    body.rotation = world.rotation;
    if (entity.Has<PrimitiveBoxComponent>())
    {
        const PrimitiveBoxComponent& box = entity.Get<PrimitiveBoxComponent>();
        body.size = {box.size.x * world.scale.x, box.size.y * world.scale.y, box.size.z * world.scale.z};
        body.color = box.color;
    }
    return body;
}

// The object's own flag. IsVisibleInWorld (object_ops.hpp) also checks the ancestors.
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
