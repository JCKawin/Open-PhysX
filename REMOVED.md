# Removed features

Open-PhysX slim-down of the Blender 5.3 tree. One tier per section. Paths were checked against this tree before anything was deleted.

DNA, RNA, blenloader, the depsgraph, EEVEE, Workbench, the GPU and draw modules, modifiers, geometry nodes, EEVEE shader nodes, bmesh, modeling, sculpt, animation, armatures, Mantaflow, cloth, soft body, Bullet, particles, Python, the window manager, OpenColorIO, and TBB stay.

## Tier 1 — CMake flags

No source directories deleted. The UCRT64 configure is `build_files/cmake/config/blender_lite.cmake`.

Already off in that file and in `build-gcc/CMakeCache.txt` before this pass:

| Requested flag | 5.3 name | Result |
| --- | --- | --- |
| WITH_CYCLES | WITH_CYCLES | off |
| WITH_CYCLES_OSL | WITH_CYCLES_OSL | off |
| WITH_CYCLES_EMBREE | does not exist; Embree is WITH_EMBREE | off |
| WITH_CYCLES_DEVICE_CUDA | WITH_CYCLES_DEVICE_CUDA | was on in the cache while Cycles itself was off; forced off |
| WITH_CYCLES_DEVICE_OPTIX | WITH_CYCLES_DEVICE_OPTIX | off |
| WITH_CYCLES_DEVICE_HIP | WITH_CYCLES_DEVICE_HIP | was on in the cache; forced off |
| WITH_CYCLES_DEVICE_ONEAPI | WITH_CYCLES_DEVICE_ONEAPI | off |
| WITH_CYCLES_DEVICE_METAL | WITH_CYCLES_DEVICE_METAL | forced off (option exists only on Apple; cache entry is harmless) |
| WITH_OPENIMAGEDENOISE | WITH_OPENIMAGEDENOISE | off |
| WITH_LIBMV | WITH_LIBMV | off |
| WITH_FREESTYLE | WITH_FREESTYLE | off |
| WITH_XR_OPENXR | WITH_XR_OPENXR | off |
| WITH_USD | WITH_USD | off |
| WITH_HYDRA | WITH_HYDRA | off |
| WITH_ALEMBIC | WITH_ALEMBIC | off |
| WITH_MATERIALX | WITH_MATERIALX | off |
| WITH_CODEC_FFMPEG | WITH_CODEC_FFMPEG | off |
| WITH_AUDASPACE | WITH_AUDASPACE | off |
| WITH_OPENAL | WITH_OPENAL | off |
| WITH_JACK | WITH_JACK | off |
| WITH_SDL | does not exist; audio flag is WITH_SDL_AUDIO | off |
| WITH_OCEANSIM | does not exist; ocean modifier is WITH_MOD_OCEANSIM | off |
| WITH_QUADRIFLOW | WITH_QUADRIFLOW | off |
| WITH_INPUT_NDOF | WITH_INPUT_NDOF | off |
| WITH_INTERNATIONAL | WITH_INTERNATIONAL | off |
| WITH_COMPOSITOR_CPU | does not exist | compositor is compiled in; `bpy.app.build_options` hard-codes it on |
| WITH_IMAGE_OPENJPEG | WITH_IMAGE_OPENJPEG | off |
| WITH_IMAGE_CINEON | WITH_IMAGE_CINEON | off |

Kept on:

- `WITH_MOD_FLUID` (there is no `WITH_FLUID`). It was off in the lite preset. Turned on so Mantaflow still builds. It does not require OpenVDB.
- `WITH_BULLET` and `WITH_JOLT`. The lite preset forced both off; the UCRT64 build had turned them on from the command line. The preset now matches that.
- `WITH_TBB` was already on. OpenColorIO has no off switch.
- `WITH_OPENVDB` and `WITH_OPENSUBDIV` stay off in this lite configure. Their source is not deleted.
- `WITH_VULKAN_BACKEND` stays on.

Toolchain:

- Installed `mingw-w64-ucrt-x86_64-ccache` and `mingw-w64-ucrt-x86_64-lld`.
- `WITH_COMPILER_CCACHE` and `WITH_LINKER_LLD` are `option()`s only inside `if(UNIX)`. The MinGW platform file still reads them. `_IS_LINKER_DEFAULT` was never set on MinGW, so the lld probe could not run. It is now set before that probe.

Files touched: `build_files/cmake/config/blender_lite.cmake`, `build_files/cmake/platform/platform_win32_mingw64.cmake`, this file.

Build: `ninja blender` in `build-gcc` succeeded. Turning ccache on changed every compile command, so this was a full rebuild: 6581 steps, 1949 seconds. No earlier full-build time was recorded, so there is no before/after comparison yet. A later incremental build is the one that should get faster. No source lines were deleted in this tier.

Smoke test (`--background`): default cube present, sculpt mode entered, fluid domain baked (`Bake Data complete`, 0.05s, 16 resolution, 2 frames), file saved and reloaded with the fluid modifier still there. `bpy.app.build_options.cycles` is false, `fluid` is true, render engine is EEVEE. The console still prints the existing "Bundled Python not found" notice because `WITH_PYTHON_INSTALL` is off and Python comes from UCRT64. The script itself raised no Python exception.

## Tier 2 — Cycles

Deleted `intern/cycles/` and `scripts/presets/cycles/`. About 246,900 lines.

`extern/libmv` does not exist. Cycles lived only under `intern/cycles`. The add-on was inside that tree (`intern/cycles/blender/addon`), not under `scripts/addons_core`.

CMake references removed, all of them already behind `if(WITH_CYCLES)` except one include path:

- `intern/CMakeLists.txt` no longer adds the cycles subdirectory.
- `CMakeLists.txt` no longer adds it for the standalone/Hydra branch.
- Include and link lines removed from `source/creator/CMakeLists.txt`, `source/blender/python/intern/CMakeLists.txt`, `source/blender/editors/render/CMakeLists.txt`, and `source/blender/makesrna/intern/CMakeLists.txt`. makesrna listed `intern/cycles/blender` even when Cycles was off.

`#include "CCL_api.h"` stays inside `#ifdef WITH_CYCLES` in `creator.cc`, `bpy_interface.cc`, and `render_texture_cache.cc`. The option `WITH_CYCLES` still exists and is off. Turning it on will not configure until the sources are restored.

Rebuild after the deletion: 196 Ninja steps, 33 seconds, `open-physx.exe` linked. The same smoke test passed again.

## Freestyle

Deleted `source/blender/freestyle/` and `scripts/freestyle/` (style modules). About 94,000 lines.

`WITH_FREESTYLE` stays off. The renderer is no longer a CMake subdirectory and is not linked. Render-pipeline hooks are gone: `RE_RenderFreestyleStrokes`, `RE_RenderFreestyleExternal`, the post-render call in `engine.cc`, `FRS_init` / `FRS_exit`, and the preview-result exception that freed the result whenever the Freestyle edge flag was set.

DNA and the view-layer data helpers stay. `FreestyleLineStyle` is still an ID, and `layer.cc` still inits, copies, and frees `view_layer->freestyle_config` through `BKE_freestyle_*` in `blenkernel/intern/freestyle.cc`. That is load/save bookkeeping, not the stroke renderer. Line-style RNA stays so old files do not drop the ID. UI panels already hide themselves when `bpy.app.build_options.freestyle` is false. Mesh mark operators stay behind `#ifdef WITH_FREESTYLE` and are not compiled.

Rebuild: 198 Ninja steps, 230 seconds. `open-physx.exe` linked.

## USD and Alembic

Deleted `source/blender/io/usd/`, `source/blender/io/alembic/`, and the import/export operators `editors/io/io_usd.cc` and `editors/io/io_alembic.cc`. About 42,600 lines (200 files in the commit, 35 insertions).

`WITH_USD` and `WITH_ALEMBIC` stay off. Those trees are no longer CMake subdirectories. Unconditional include paths that pointed at them were removed from `editors/io/CMakeLists.txt`, `makesrna/intern/CMakeLists.txt` (`../../io/usd`), and `source/creator/CMakeLists.txt`. `WM_OT_usd_*` and `WM_OT_alembic_*` are no longer registered. The top bar already hid those entries when `bpy.app.build_options.usd` and `.alembic` are false.

Cache modifier and constraint DNA stay so old files still load. `MeshSeqCacheModifierData` and `bTransformCacheConstraint` are still in DNA, and their type-info slots still copy and free the `CacheFile` pointer. With the readers gone, evaluate is a no-op (the read calls were already inside `#ifdef WITH_USD` / `WITH_ALEMBIC`). Mesh Sequence Cache is marked `eModifierTypeFlag_NoUserAdd` and dropped from the modifier add menu. Transform Cache is filtered out of the object and pose constraint add menus. The older Mesh Cache modifier (MDD/PC2) is unrelated and stays. `CacheFile` operators stay so a loaded cache datablock can still be freed.

Hydra's CMake file still names `bf_io_usd`, and `WITH_HYDRA` stays off because it depends on USD. `rna_usd.cc` stays on disk and is not compiled.

Rebuild: 215 Ninja steps, 48 seconds. `open-physx.exe` linked. The previous Freestyle rebuild was 230 seconds; this one is shorter because ccache covered the makesrna regeneration.

## QuadriFlow

Deleted `extern/quadriflow/` (including the bundled LEMON library) and `intern/quadriflow/`. About 121,000 lines (250 files in the commit, 8 insertions).

`WITH_QUADRIFLOW` stays off. Those trees are no longer CMake subdirectories, and `bf_blenkernel` no longer links `bf_intern_quadriflow`. `OBJECT_OT_quadriflow_remesh` is gone, along with `BKE_mesh_remesh_quadriflow`. The mesh data properties panel no longer offers a Quad mode; it only shows Voxel Remesh. `REMESH_QUAD` stays in DNA and in the RNA enum so an old file that stored that mode still loads. `WM_JOB_TYPE_QUADRIFLOW_REMESH` stays in the job-type enum so later values do not shift. Voxel remesh is unchanged.

Rebuild: 38 Ninja steps, 52 seconds. `open-physx.exe` linked. The previous USD rebuild was 215 steps and 48 seconds.

## Compositor — Stage A

Unregistered the compositor from the UI and from node-type registration. No implementation files were deleted. DNA structs stay, including compositor node storage, scene effects, and `CompositorNodeTree`.

- `register_compositor_nodes()` is no longer called. The compositor node-tree type stays registered so stored trees still have an idname. Its `poll` returns false, so the node editor does not offer Compositor as a tree type.
- `ED_operatortypes_scene_compositor` and `ED_menutypes_scene_compositor` are no longer called. `NODE_OT_duplicate_compositing_node_group` is gone because it called one of those operators.
- `bl_ui` no longer loads `node_add_menu_compositor` or `properties_compositor`. The properties editor no longer has the Compositor & Effects tab. The EEVEE compositor performance panels are not registered. The node Add and Swap menus no longer insert the compositor catalogs.

`bNodeType::is_type` used to assert that the queried idname was registered. Shader and geometry trees call it with `CompositorNodeOutputFile` and `CompositorNodeViewer` on every tree update, so leaving the compositor types unregistered aborted in `BKE_materials_init`. The assert was removed. The idname compare stays.

Factory startup (`--background --factory-startup`) exits 0. `SCENE_OT_add_compositor_effect` and `NODE_MT_compositor_node_add_all` are absent. `CompositorNodeTree` RNA is still there.

Stage B is not done. `source/blender/compositor/`, `source/blender/nodes/composite/`, the draw compositor engine, `render/intern/compositor.cc`, and the sequencer compositor effect still reference that code. Deleting those files would touch far more than the call sites that can be stubbed in one pass.

Rebuild after the header change: 407 Ninja steps, 409 seconds. `open-physx.exe` linked. The unregister itself, before the `is_type` edit, was 10 steps and 20 seconds. The previous QuadriFlow rebuild was 38 steps and 52 seconds.
