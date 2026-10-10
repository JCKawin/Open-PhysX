#pragma once

#include "ecs/scene.hpp"

#include <vector>

namespace openphysx {

// Scene operations behind the object keys (epic A2 to A5). They change the scene and
// nothing else: no window, GPU, or UI types. The editor wraps each call in a command.

// True when the entity and every ancestor are visible.
bool IsVisibleInWorld(const Scene& scene, const Entity& entity);

// Objects the viewport draws and can pick, in scene order.
std::vector<UUID> DrawableObjects(const Scene& scene);

// Selected objects, in scene order.
std::vector<UUID> SelectedObjects(const Scene& scene);

// Selected, visible objects whose ancestors are not selected. The transform tools move
// these. A selected child follows its selected parent, so it is not a root.
std::vector<UUID> TransformRoots(const Scene& scene);

void SetSelected(Scene& scene, UUID id, bool selected);
void DeselectAll(Scene& scene);
// Replaces the selection with every visible object.
void SelectAllVisible(Scene& scene);

// The active object is a member of the selection, as in Blender. Keeps `active` when it is
// selected. Otherwise falls back to the last selected object, or kNullUuid when none is.
UUID ResolveActive(const Scene& scene, UUID active);

// Hides the selected objects. The selection itself does not change.
void HideSelected(Scene& scene);
// Shows every hidden object and selects it.
void RevealHidden(Scene& scene);

// Removes the objects. A child that is not removed keeps its world transform and loses its
// parent (parent_id = 0). It is not deleted with the parent.
void DeleteObjects(Scene& scene, const std::vector<UUID>& ids);

// Copies each object with its components by value, under a new id and a unique name. A copy
// keeps the source's parent, or the copy of that parent when the parent is copied too. The
// sources are deselected and the copies selected. Returns the copies in scene order.
std::vector<UUID> DuplicateObjects(Scene& scene, const std::vector<UUID>& ids);

// Adds a 2 m box beside the existing objects, selects it alone, and returns its id.
UUID AddBoxObject(Scene& scene);

} // namespace openphysx
