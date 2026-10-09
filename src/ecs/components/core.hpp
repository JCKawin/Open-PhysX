#pragma once

#include "core/Types.h"
#include "ecs/uuid.hpp"

#include <string>
#include <vector>

namespace openphysx {

struct IDComponent
{
    UUID id = kNullUuid;
};

struct TagComponent
{
    std::string name = "Entity";
};

// Local transform. Float, matching the rest of the engine.
struct TransformComponent
{
    Vec3 position{};
    Quat rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

// Parent and children are UUIDs. Zero parent means no parent. Child order is kept.
struct RelationshipComponent
{
    UUID parent = kNullUuid;
    std::vector<UUID> children;
};

struct EditorStateComponent
{
    bool visible = true;
    bool locked = false;
};

inline bool operator==(const IDComponent& a, const IDComponent& b)
{
    return a.id == b.id;
}

inline bool operator==(const TagComponent& a, const TagComponent& b)
{
    return a.name == b.name;
}

inline bool operator==(const TransformComponent& a, const TransformComponent& b)
{
    return a.position == b.position && a.rotation == b.rotation && a.scale == b.scale;
}

inline bool operator==(const RelationshipComponent& a, const RelationshipComponent& b)
{
    return a.parent == b.parent && a.children == b.children;
}

inline bool operator==(const EditorStateComponent& a, const EditorStateComponent& b)
{
    return a.visible == b.visible && a.locked == b.locked;
}

} // namespace openphysx
