# Engine path

Backlog from the current tree to a Blender-shaped physics workstation. Behavior follows Blender’s modules: object data, modifiers, constraints, rigid body, point cache, fluid, OpenVDB, gizmos, and mesh IO.

The code stays in `src/` under the existing split: `core/`, `logic/`, `renderer/`, `ui/`. Reimplement the same data, the same evaluation order, and the same user-facing results. Do not paste Blender sources in. `logic/` still must not include raylib or ImGui.

What is already built is [architecture.md](architecture.md). What the machine should do once a solver exists is [hardware.md](hardware.md). This page is only the remaining work, in the order it has to happen.

## Names

**ECS** means Blender’s object model, not a third-party entity framework. An object is the entity. Transform, mesh, rigid body, constraints, modifiers, and fluid role are the components. Blender stores those on `Object` and evaluates them in order. That is the layout to build.

**Gimbal** means transform gizmos (Blender’s move / rotate / scale gizmos in `windowmanager/gizmo`). Camera gimbal lock is already handled by `View3D` quaternions.

**VBD cache** means **OpenVDB** (`.vdb`) volume caches, which is how Blender stores fluid grids. Rigid bodies use Blender’s point cache. That is a separate slot.

Story points are relative scope for one person on this repo: 1 small, 2 a few days, 3 about a week, 5 two weeks, 8 a month of one area, 13 a multi-month milestone. They are not a schedule.

## Already finished

Do not rebuild this.

- Docked workstation, theme, playback clock, fixed `1/60` step, one cube, demo motion.
- `View3D`: quaternion orbit, pan, zoom-to-cursor, orthographic toggle, numpad axis snaps, frame all, smooth view. Mouse preset RMB or MMB.
- Object-mode keymap: `G` / `R` / `S`, axis lock, precision, snap, numeric entry, hide, undo of pose / hide / reset.
- `IRenderer` has no raylib types. `draw_viewport_image()` owns the blit.

Not finished, even where the UI looks close: many objects, meshes, gizmos, edit mode, outliner, file open/save, modifiers, constraints, any solver, any cache, import.

## Order

Do the epics in order. A later epic may stub its data early. It does not get a solver until the epic that owns that solver. Each id links to its own guide: what to do, how to do it, what to consider, the output, and the next stage.

```text
Scene objects
  -> mesh + OBJ/STL import
  -> gizmos and physics UI
  -> modifier stack and object constraints
  -> arenas + rigid world + point cache + joints
  -> fluid roles + OpenVDB cache
  -> cloth / soft body on the same cache
```

Pointed work is 131 points through fluid cache. Cloth, edit mode, and heavy importers come after the two caches round-trip.

## Epic A — Scene objects (the ECS) — 13

Blender spec: `DNA_object_types.h`, `BKE_main`, `BKE_lib_id`, collections in `BKE_collection`.

| ID | Story | Pts |
| --- | --- | --- |
| [A1](epica/a1.md) | `Scene` owns a list of objects and one active object. The demo cube is object 0. `SimulationState` stays the transport and display block. | 3 |
| [A2](epica/a2.md) | Each object has a stable id, name, transform (location, quaternion, scale), visibility, selection, and a parent pointer. Selection is a set, not one bool on the cube. | 3 |
| [A3](epica/a3.md) | Components on the object, empty at first: mesh slot, rigid-body settings, constraint list, modifier list, fluid role. Same split Blender uses. Rigid body is not a modifier. | 2 |
| [A4](epica/a4.md) | Outliner panel: name, visibility, selection, parent. Click selects. Box select and `A` operate on the list. | 3 |
| [A5](epica/a5.md) | Duplicate, delete, hide, and undo apply to objects, not to a single hardcoded cube. | 2 |

## Epic B — Mesh and import — 21

Blender spec: `BKE_mesh`, `io/wavefront_obj`, `io/stl`, `io/ply`, `IO_orientation`. This project is Y-up. Many files are Z-up. Convert at the boundary.

| ID | Story | Pts |
| --- | --- | --- |
| [B1](epicb/b1.md) | Mesh component: positions, indices, normals. Original mesh is never written by modifiers. | 5 |
| [B2](epicb/b2.md) | File Open/Save for the workstation scene (our format, not `.blend`). The disabled menu items become real. | 5 |
| [B3](epicb/b3.md) | Import OBJ and STL into mesh objects. Convert axes and units at the boundary. One object per mesh. | 5 |
| [B4](epicb/b4.md) | Import PLY. Export OBJ of the original mesh, not the simulated pose. | 3 |
| [B5](epicb/b5.md) | FBX, Alembic, and USD stay later. They are Blender IO modules, not part of the first physics path. | 3 |

## Epic C — Gizmos and UI polish — 13

Blender spec: `windowmanager/gizmo`, `space_view3d/view3d_gizmo_*`, transform orientation.

| ID | Story | Pts |
| --- | --- | --- |
| [C1](epicc/c1.md) | Move, rotate, and scale gizmos drawn in the viewport for the active object. They call the same operators as `G` / `R` / `S`. | 5 |
| [C2](epicc/c2.md) | Global / local orientation. Snapping stays on Ctrl. The gizmo respects the axis lock. | 2 |
| [C3](epicc/c3.md) | Properties tabs in Blender’s order: Object, Modifiers, Constraints, Physics. Empty tabs until later epics fill them. | 3 |
| [C4](epicc/c4.md) | Viewport shading: bounds, wire, solid. Overlay toggles for grid, gizmos, and later for collisions and cache. | 2 |
| [C5](epicc/c5.md) | Status text for mode, tool, and gizmo axis. Object mode only until a mesh edit mode exists. | 1 |

## Epic D — Evaluation order — 8

Blender spec: depsgraph object evaluation, reduced to a fixed graph with the same stages. Not a copy of every depsgraph node.

| ID | Story | Pts |
| --- | --- | --- |
| [D1](epicd/d1.md) | Per object: constraints, then modifiers, then physics writes transforms back. Original data stays. | 3 |
| [D2](epicd/d2.md) | UI writes commands. The step drains them. The viewport reads a published snapshot. | 3 |
| [D3](epicd/d3.md) | Cap substeps per display frame at 8. Keep the playhead interpolation alpha from [hardware.md](hardware.md). | 2 |

## Epic E — Modifier stack — 13

Blender spec: `ModifierData` and `ModifierTypeInfo` in `BKE_modifier.hh`.

| ID | Story | Pts |
| --- | --- | --- |
| [E1](epice/e1.md) | Linked list, enable flags (viewport, render), reorder, one active modifier, undo of stack edits. | 3 |
| [E2](epice/e2.md) | Vtable: deform in place, or build a new mesh. Original mesh untouched. | 3 |
| [E3](epice/e3.md) | One deform modifier (displace or wave) so the viewport can show original versus evaluated. | 2 |
| [E4](epice/e4.md) | Collision modifier. Single instance. Marks the object as a collider and reads the mesh above it. | 3 |
| [E5](epice/e5.md) | Fluid modifier slot, empty until Epic H. Cloth and soft body wait until the point cache exists. | 2 |

## Epic F — Object constraints — 8

Blender spec: `bConstraint` list, `BKE_constraint.h`. These run before the solver. They are not joints.

| ID | Story | Pts |
| --- | --- | --- |
| [F1](epicf/f1.md) | Stack header: type, target object, influence, mute, order, undo. | 2 |
| [F2](epicf/f2.md) | Child Of, Copy Location, Copy Rotation, Copy Scale. | 3 |
| [F3](epicf/f3.md) | Limit Location, Limit Rotation, Limit Scale. | 2 |
| [F4](epicf/f4.md) | Skip IK, spline IK, shrinkwrap, and camera solvers until an armature exists. | 1 |

## Epic G — Rigid body, as Blender wires it — 34

Blender spec: `DNA_rigidbody_types.h`, `BKE_rigidbody_do_simulation`, `intern/rigidbody/RBI_api.h`, `BKE_pointcache.h`.

The world, the step loop, and the cache match Blender. The contact solver behind that wall starts as the scalar pipeline in [hardware.md](hardware.md): integrate, broadphase, narrowphase, islands, iterations, sleep. Bullet may sit behind the same wall later as a parity backend. Bullet types do not leak into `core/` or `ui/`.

| ID | Story | Pts |
| --- | --- | --- |
| [G1](epicg/g1.md) | Frame arena and fixed-size pool. `step()` does not call the global heap. | 3 |
| [G2](epicg/g2.md) | `RigidBodyWorld` on the scene: gravity, time scale, substeps (default 10), iterations, mute, last simulated time. | 3 |
| [G3](epicg/g3.md) | Body component: active or passive, kinematic, mass, friction, restitution, damping, sleep, axis locks. Shapes: box, sphere, then convex hull and triangle mesh from the modifier result. | 5 |
| [G4](epicg/g4.md) | Scalar integrate. The demo sine stops being the motion. The world steps forward only, one frame at a time. | 5 |
| [G5](epicg/g5.md) | Broadphase, narrowphase, islands, solver iterations, sleep. Box and sphere first. | 8 |
| [G6](epicg/g6.md) | Point cache for rigid transforms. An exact frame hit skips the solver. Scrubbing backward reads the cache. A setting change marks the cache outdated. | 5 |
| [G7](epicg/g7.md) | Joints, solved in the world: Fixed, Point, Hinge, Slider, Piston, 6DoF, 6DoF spring, Motor. Enable, break, and per-axis limits. | 5 |

## Epic H — Fluid and OpenVDB cache — 21

Blender spec: `DNA_fluid_types.h`, `BKE_fluid_modifier_do`, `BKE_fluid_get_velocity_at`, OpenVDB cache on the fluid domain. Mantaflow is the reference behavior, not a library to vendor in the first fluid story.

| ID | Story | Pts |
| --- | --- | --- |
| [H1](epich/h1.md) | Fluid modifier roles: Domain (gas first), Flow (inflow / outflow / geometry), Effector (collision). One domain. | 3 |
| [H2](epich/h2.md) | CPU gas step on a fixed grid, capped near 32³. Semi-Lagrangian advection and a simple pressure solve. | 8 |
| [H3](epich/h3.md) | OpenVDB cache: one density grid and one velocity grid per frame. Scrub reads `.vdb`. Setting changes free the cache. | 5 |
| [H4](epich/h4.md) | Viewport debug draw of the grid. `velocity_at(position)` for later coupling. | 3 |
| [H5](epich/h5.md) | Effectors follow rigid transforms (one-way). Liquid surface, fire, noise, and adaptive domain come after gas caches round-trip. | 2 |

## After the two caches

Same Blender modules. Not scheduled until Epic G and Epic H caches round-trip.

- Cloth and soft body as single modifiers with the point cache (`MOD_cloth`, `MOD_softbody`).
- Mesh edit mode (vertex / edge / face) so collision shapes can be edited. Knife, bevel, and loop cut stay behind that.
- FBX, Alembic, and USD import.
- SIMD and a job system, then a GPU broadphase, from [hardware.md](hardware.md).
- Two-way fluid force on rigid bodies through `velocity_at`.

## What we still do not copy

DNA/RNA as the file format, the Python operator API, EEVEE, Cycles, grease pencil, sculpt, the compositor, and Mantaflow as a vendored build. Scene save is our format. Import follows Blender’s mesh IO behavior, not `.blend` loading.
