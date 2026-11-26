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
