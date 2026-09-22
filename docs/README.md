# Open PhysX documentation

This folder is the maintained explanation of the software: what exists now, how a frame runs, how the tree is built, and how to take the same core to every platform while using the hardware that is actually there.

**This is not NVIDIA PhysX.** Open PhysX is an independent GPL-3 workstation and (eventually) solver. The name is the project name, not a wrapper around the proprietary engine.

## How to read this set

| You want… | Open |
| --- | --- |
| A map of every doc and which source it tracks | [diagrams/00-doc-map.drawio](diagrams/00-doc-map.drawio) + this page |
| What the program *is* today (modules, files, UI) | [architecture.md](architecture.md) + [01](diagrams/01-system-architecture.drawio) + [02](diagrams/02-runtime-loop.drawio) |
| How CMake pulls raylib / ImGui and produces a binary | [diagrams/03-build-graph.drawio](diagrams/03-build-graph.drawio) |
| Run on Windows, Linux, macOS, the web, and headless | [portability.md](portability.md) + [04](diagrams/04-portability.drawio) |
| Saturate CPU cores, SIMD, and GPU compute | [hardware.md](hardware.md) + [05](diagrams/05-hardware.drawio) |
| Rules so the diagrams do not rot | [maintenance.md](maintenance.md) |

Markdown is the **why / how / spec**. Draw.io is the **structure**. GitHub will render the mermaid sketches in the `.md` files; the `.drawio` files are the ones you rearrange in [diagrams.net](https://app.diagrams.net/) or the VS Code / JetBrains draw.io plugins.

## Draw.io files (one concern each)

Each `.drawio` is a small multi-page file. Pages are tabs inside the editor.

| File | Pages | Owns |
| --- | --- | --- |
| [00-doc-map.drawio](diagrams/00-doc-map.drawio) | Doc map | Which markdown + source file a diagram tracks |
| [01-system-architecture.drawio](diagrams/01-system-architecture.drawio) | Overview, Source layout, Contracts | Modules, folders, `ISimulation` / `IRenderer` |
| [02-runtime-loop.drawio](diagrams/02-runtime-loop.drawio) | Lifecycle, Frame + timestep, Workspace UI, Data model | `Application::frame`, dock layout, `Types.h` |
| [03-build-graph.drawio](diagrams/03-build-graph.drawio) | Dependencies, Configure and run | `CMakeLists.txt`, FetchContent, Emscripten stub |
| [04-portability.drawio](diagrams/04-portability.drawio) | Platform matrix, Backend stack, Build & deploy | “Run everywhere” target layering |
| [05-hardware.drawio](diagrams/05-hardware.drawio) | Hardware probe, Threads & jobs, Physics pipeline, Scaling tiers | “Use the machine” target |

Color language is the same in every diagram:

- Orange — application shell
- Blue — simulation / logic
- Green — renderer (or “implemented now”)
- Purple — UI
- Yellow — shared data
- Grey — third-party or “not this version”
- Red — GPU / danger / contract leak
- Cyan — CPU / platform
- Orange dashed (in the portability and hardware files) — **target, not shipped**

## Current software in one paragraph

`main` constructs `openphysx::Application`. That object owns a `Simulation`, a `Renderer`, and a `Workspace` by value. The window comes from **raylib 6.0** (OpenGL, GLFW). The chrome is **Dear ImGui** via **rlImGui**. Each display frame accumulates `GetFrameTime()` and steps the simulation at a fixed `1/60 s`. The simulation is a playback clock plus an optional sine offset on a single cube. The interesting part is the split: `ISimulation` already forbids window/GPU/UI types, which is the seam for a real solver, extra platforms, and extra compute backends.

## Build and run (today)

```bash
cmake -S . -B build
cmake --build build
```

The executable is `Open_PhysX` (`Open_PhysX.exe` on Windows). First configure needs the network (FetchContent for raylib and rlImGui). ImGui is vendored under `external/imgui/`.

Needs: CMake ≥ 3.25, a C++20 compiler, an OpenGL 3.3-class GPU. No CUDA/Vulkan SDK is required for the current binary.

## Honest limits (do not document as if they existed)

- One thread does input, step, ImGui, and GL submit.
- There is no collision, constraint solver, or body islanding.
- Tools (Select / Move / Rotate / Scale) are radio buttons only.
- File Open/Save and Undo/Redo are disabled menu stubs.
- `IRenderer.h` includes `raylib.h`, so the render contract is not portable yet.
- Emscripten flags exist in CMake; there is no shipped web product.

Those limits are why [portability.md](portability.md) and [hardware.md](hardware.md) are written as a **target architecture on top of the existing contracts**, not as a description of running code.
