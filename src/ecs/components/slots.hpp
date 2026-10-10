#pragma once

#include <vector>

namespace openphysx {

// Empty slots for later epics. Nothing evaluates them yet, and none of these types has a
// step() method. Evaluation order belongs to the solver epic, not to the component.
//
// Blender mapping (epic A3):
//   mesh slot        -> MeshRendererComponent::meshAsset (kNullUuid means empty)
//   rigid body slot  -> RigidBodyComponent present or absent
//   constraint stack -> ConstraintStackComponent, always present, empty list
//   modifier stack   -> ModifierStackComponent, always present, empty list
//   fluid role       -> FluidRoleComponent, always present, None

// Fluid lives on the object as a value. Blender keeps fluid on a modifier. The split is
// kept so rigid bodies and fluid never share one component.
enum class FluidRole
{
    None,
    Domain,
    Flow,
    Effector,
};

struct FluidRoleComponent
{
    FluidRole role = FluidRole::None;
};

// Fields arrive with the constraint epic. An empty list means "no constraints", which is
// different from the component being absent.
struct ConstraintEntry
{
};

struct ConstraintStackComponent
{
    std::vector<ConstraintEntry> entries;
};

// Fields arrive with the modifier epic.
struct ModifierEntry
{
};

struct ModifierStackComponent
{
    std::vector<ModifierEntry> entries;
};

inline bool operator==(const FluidRoleComponent& a, const FluidRoleComponent& b)
{
    return a.role == b.role;
}

inline bool operator==(const ConstraintEntry&, const ConstraintEntry&)
{
    return true;
}

inline bool operator==(const ConstraintStackComponent& a, const ConstraintStackComponent& b)
{
    return a.entries == b.entries;
}

inline bool operator==(const ModifierEntry&, const ModifierEntry&)
{
    return true;
}

inline bool operator==(const ModifierStackComponent& a, const ModifierStackComponent& b)
{
    return a.entries == b.entries;
}

} // namespace openphysx
