# Open PhysX

An open GPL-3 3D physics **workstation** (early scaffold). Not NVIDIA PhysX.

Right now the binary is a dockable editor: raylib 6.0 window, Dear ImGui chrome, one cube on a grid, a playback clock, and a fixed `1/60 s` step. The useful part is the split — `ISimulation` has no GPU or UI types — which is the seam for a real solver and for running the same core on desktop, web, and headless machines.

## Docs

Start at **[docs/README.md](docs/README.md)**.

| Topic | Markdown | Draw.io |
| --- | --- | --- |
| What the code is | [docs/architecture.md](docs/architecture.md) | [01](docs/diagrams/01-system-architecture.drawio), [02](docs/diagrams/02-runtime-loop.drawio) |
| Build graph | (see architecture + portability) | [03](docs/diagrams/03-build-graph.drawio) |
| Run everywhere | [docs/portability.md](docs/portability.md) | [04](docs/diagrams/04-portability.drawio) |
| Use all the hardware | [docs/hardware.md](docs/hardware.md) | [05](docs/diagrams/05-hardware.drawio) |
| How to keep this set honest | [docs/maintenance.md](docs/maintenance.md) | [00](docs/diagrams/00-doc-map.drawio) |

Open the `.drawio` files in [diagrams.net](https://app.diagrams.net/) or a draw.io editor plugin. GitHub will render the mermaid sketches inside the markdown.

## Build

Needs CMake ≥ 3.25, a C++20 compiler, and an OpenGL 3.3-class GPU. First configure downloads raylib and rlImGui (ImGui is already under `external/imgui`).

```bash
cmake -S . -B build
cmake --build build
```

The executable is `Open_PhysX` (`Open_PhysX.exe` on Windows).

Viewport: RMB orbit, Shift+RMB pan, wheel zoom. `Ctrl+N` resets the simulation, `Ctrl+Q` quits.

## License

[GPL-3.0](LICENSE).
