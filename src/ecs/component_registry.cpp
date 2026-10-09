#include "ecs/component_registry.hpp"

#include "ecs/components/serialize.hpp"

#include <type_traits>

namespace openphysx {

ComponentRegistry& ComponentRegistry::Instance()
{
    static ComponentRegistry registry;
    return registry;
}

const ComponentInfo* ComponentRegistry::Find(std::string_view name) const
{
    for (const ComponentInfo& info : all_)
    {
        if (info.name == name)
            return &info;
    }
    return nullptr;
}

ComponentInfo* ComponentRegistry::FindMutable(std::string_view name)
{
    for (ComponentInfo& info : all_)
    {
        if (info.name == name)
            return &info;
    }
    return nullptr;
}

void EnsureComponentsRegistered()
{
    static bool ready = false;
    if (ready)
        return;
    ready = true;

    ComponentRegistry& registry = ComponentRegistry::Instance();
    const ComponentOptions saved{};
    const ComponentOptions editor_only{.serializable = false, .replicate = true, .compares = true};
    const ComponentOptions runtime_only{.serializable = false, .replicate = false, .compares = false};

    registry.Register<IDComponent>("ID", 1, saved);
    registry.Register<TagComponent>("Tag", 1, saved);
    registry.Register<TransformComponent>("Transform", 1, saved);
    registry.Register<RelationshipComponent>("Relationship", 1, saved);
    registry.Register<EditorStateComponent>("EditorState", 1, saved);
    registry.Register<PrimitiveBoxComponent>("PrimitiveBox", 1, saved);
    registry.Register<MeshRendererComponent>("MeshRenderer", 1, saved);
    registry.Register<MaterialOverrideComponent>("MaterialOverride", 1, saved);
    registry.Register<RigidBodyComponent>("RigidBody", 1, saved);
    registry.Register<ColliderComponent>("Collider", 1, saved);
    registry.Register<PhysicsMaterialComponent>("PhysicsMaterial", 1, saved);
    registry.Register<JointComponent>("Joint", 1, saved);
    registry.Register<RobotComponent>("Robot", 1, saved);
    registry.Register<LinkComponent>("Link", 1, saved);
    registry.Register<ActuatorComponent>("Actuator", 1, saved);
    registry.Register<SensorComponent>("Sensor", 1, saved);
    registry.Register<CfdDomainComponent>("CfdDomain", 1, saved);
    registry.Register<SelectionOutlineTag>("SelectionOutline", 1, editor_only);
    registry.Register<MeshGpuHandle>("MeshGpuHandle", 1, runtime_only);
    registry.Register<PhysicsBodyHandle>("PhysicsBodyHandle", 1, runtime_only);
    registry.Register<ContactCache>("ContactCache", 1, runtime_only);
    registry.Register<CfdResultField>("CfdResultField", 1, runtime_only);
}

} // namespace openphysx
