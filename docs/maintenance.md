# Maintaining the diagrams and the software together

The documentation set is split so a change to one concern does not require redrawing everything. This page is the rulebook.

## Which format owns what

| Kind of change | Edit | Do not |
| --- | --- | --- |
| A box moves, a new module appears, a pipeline stage is added | the matching **`.drawio` tab** | dump a new giant file |
| Why a choice was made, how to build, order of work, mermaid for GitHub | the matching **`.md`** | put essays inside draw.io boxes |
| A function signature or default value | **source** first, then the row in architecture.md / the Contracts tab | leave the diagram claiming `visual_body` does something else |
| A target that is not in the tree yet | portability.md / hardware.md and 04 / 05, marked as target | draw it in 01 Overview as if it shipped |

Draw.io is for **structure**. Markdown is for **explanation**. If a box needs a paragraph, the paragraph belongs in `.md` and the box stays a title plus one line.

## File ownership

Visual index: [diagrams/00-doc-map.drawio](diagrams/00-doc-map.drawio).

| Concern | Draw.io | Markdown | Source of truth in code |
| --- | --- | --- | --- |
| Modules, folders, `I*` contracts | `01-system-architecture.drawio` | `architecture.md` | `src/**`, especially the `I*` headers |
| Frame, timestep, dock UI, `Types.h` | `02-runtime-loop.drawio` | `architecture.md` | `Application.cpp`, `Workspace.cpp`, `core/Types.h` |
| CMake, FetchContent, flags | `03-build-graph.drawio` | `portability.md` (build section) | `CMakeLists.txt` |
| Platforms, backends, CI, packaging | `04-portability.drawio` | `portability.md` | future `IPlatform` / presets / CI yaml |
| Probe, threads, solver stages, tiers | `05-hardware.drawio` | `hardware.md` | future job system / `World` |
| This mapping | `00-doc-map.drawio` | this file | — |

One concern per `.drawio` file. Multiple **tabs** (pages) inside a file are allowed and expected. Do not add a seventh file because a tab got full — add a tab, or split only when a file would mix two concerns (for example do not put the physics pipeline into `01`).

## Color language (keep it stable)

Reuse these fills so a reader can scan every file:

| Fill / stroke | Meaning |
| --- | --- |
| `#FFE6CC` / `#D79B00` | Application shell, or **target** (dashed stroke) |
| `#DAE8FC` / `#6C8EBF` | Logic / simulation |
| `#D5E8D4` / `#82B366` | Renderer, or **implemented now** |
| `#E1D5E7` / `#9673A6` | UI |
| `#FFF2CC` / `#D6B656` | Shared data / notes |
| `#F5F5F5` / `#666666` | Third-party |
| `#F8CECC` / `#B85450` | GPU, contract leak, danger |
| `#B1DDF0` / `#368087` | CPU / platform |
| `#303030` / `#1D1D1D` | Title bars |
| `#E6E6E6` / `#888888` | Muted / out of scope |

If you add a backend, pick an existing role color. Do not invent a sixth palette.

## How to edit a `.drawio`

1. Open the file in [diagrams.net](https://app.diagrams.net/) (File → Open from → Device) or in a draw.io editor plugin.
2. Use the **tabs** at the bottom; do not paste a second `<mxfile>` into the same path.
3. Keep boxes on the 10 px grid. Page size is 1600×1100 (or 1700×1200 for the wide matrices). If you need more width, grow the page, do not shrink fonts below 11 px.
4. Save **uncompressed XML** (diagrams.net default when the file is `.drawio` text, not `.drawio.png`). These files are git-diffable. Do not check in a binary `.png` export as the source.
5. After saving, glance at the matching `.md` section. If a default (`duration = 10`, `kFixedDt = 1/60`, raylib **6.0**) changed, update both.

## When a code change must touch docs

Treat this as part of the same patch:

| Code change | Docs |
| --- | --- |
| New folder under `src/` | 01 Source layout + architecture.md file map |
| New method on `ISimulation` / `IRenderer` | 01 Contracts |
| Window flags, timestep, init order | 02 Lifecycle / Frame |
| New ImGui panel or dock split | 02 Workspace UI |
| New CMake option or dependency | 03 + portability.md |
| First job thread, SIMD kernel, or GPU dispatch | 05 + hardware.md, and mark the Overview note on 01 if “one thread” is no longer true |
| A menu item that actually works (Open/Save, gizmos) | 02 UI + architecture.md UI table |

If you skip the diagram because “it is only a prototype,” the next reader will trust the picture and the picture will be wrong.

## NOW vs TARGET

Pages in `01`–`03` describe **the tree as compiled**. Pages in `04`–`05` describe **the intended shape**. Target boxes use orange fill and a dashed stroke. When a target ships:

1. Restyle the box to the role color and solid stroke.
2. Move the “Status now” cell on the platform matrix to green.
3. Delete the corresponding “not present” sentence in the markdown.
4. Do **not** leave a second, stale copy of the old dashed box.

## What not to do

- Do not resurrect a single `code_architecture.drawio` mega-file. That is why this set is split.
- Do not generate these files from a script after they have been hand-edited in diagrams.net. The XML in `docs/diagrams/` is the source.
- Do not put NVIDIA PhysX types, CUDA headers, or `raylib.h` into `logic/`. The docs will fight you, correctly.
- Do not mark a GPU path as “NOW” until a headless test steps it.

## Opening the diagrams on GitHub

GitHub does not render `.drawio`. Use:

- the mermaid sketches in the `.md` files for PR review
- diagrams.net “Open from GitHub” if you want the real graph
- a local editor plugin for day-to-day work

If you need a PNG for a slide, export from diagrams.net **as an extra artifact**, do not replace the `.drawio`.
