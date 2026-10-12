# currentstate.md — Open-PhysX System State & Implementation Log

This document tracks all major architecture and implementation changes made across the project so subsequent agents and developers have immediate context without needing to inspect the entire codebase.

---

## 1. Executive Summary & Current Status

- **Build Toolchain:** CLion Ninja (`C:\Program Files\JetBrains\CLion 2026.2.1\bin\ninja\win\x64\ninja.exe`), Clang++ (`C:\msys64\clang64\bin\clang++.exe`), C++23.
- **Active Build Dir:** `cmake-build-debug-mingw`
- **Test Suite:** `Open_PhysX_tests.exe` (all 70 test cases pass, 765 assertions verified).
- **Architecture Integrity:** Strict layering enforced. Core and IO layers (`src/core`, `src/io`, `src/logic`) do **not** depend on raylib or ImGui. Renderer and UI layer bridges `Vec3`/`Mesh` to raylib GPU constructs (`::Model`, `UploadMesh`).

---

## 2. Completed Implementations & Verification

### Epic B1: Mesh Component (`src/core/Mesh.h`, `src/core/Mesh.cpp`)
- **`Mesh` Data Model:** Holds `positions` (`std::vector<Vec3>`), `indices` (`std::vector<uint32_t>`, 3 per triangle, CCW winding), and per-vertex `normals` (`std::vector<Vec3>`).
- **Validation:** `Mesh::validate()` checks `indices.size() % 3 == 0`, bounds check for all indices against vertex count, and matching/empty normal sizes.
- **Vertex Normal Generation:** Area-weighted face normal accumulation into corners; handles degenerate zero-area faces safely without producing `NaN` (falls back to `(0, 1, 0)`). Computed once on import/creation, never per-frame.
- **Cube Primitive (`make_cube`):** 24 vertices (4 per face, unshared) and 12 triangles (Decision D1). Face normals point outward from center. Bounds check confirmed.
- **Scene Mesh Library:** `Scene` now owns `std::vector<Mesh>` with 1-based indexing (`add_mesh(m)`, `find_mesh(id)`), revision tracking (`mesh_revision_`).
- **Undo Integration:** Scene copy/move operators preserve mesh library.
- **GPU Mesh Cache (`src/renderer/MeshCache.h/.cpp`):** Converts `const Mesh&` into raylib `::Model` via de-indexing (safe against 16-bit vertex limits), cached per `(mesh_id, mesh_revision)`.
- **Renderer Integration:** `Renderer::render()` draws `::Model` when an entity's `PrimitiveBoxComponent.mesh_id > 0`, falling back cleanly to the box primitive when `mesh_id == 0`.

### Epic B2: Persisted Binary IO Foundation (`src/io/Binary.h`)
- **`Writer`:** Appends `u8`, `u32`, `u64`, `f32` (IEEE-754 bit-cast), length-prefixed UTF-8 strings, and 4-byte chunk tags with back-patched length headers.
- **`Reader`:** Bounds-checked reader. Prevents buffer overrun, allocation bombs, and handles sub-readers safely.

### Epic B3 & B4: Import & Export Pipeline (`src/io/`)
- **Coordinate Boundary (`src/io/Import.h`):** `z_up_to_y_up()` converts CAD/Blender Z-up `(x, y, z)` into `(x, z, -y)` with determinant $+1$ (preserves CCW winding).
- **Format Detection & `.blend` Rejection (`src/io/FormatDetect.h/.cpp`):**
  - Detects `.obj`, `.stl`, `.ply`, `.glb`, `.fbx`, `.abc`.
  - Rejects `.blend` (sniffing `BLENDER` magic or `.blend`/`.blend1` extension) with the required behavioral instruction message.
- **Vertex Welding (`src/io/Weld.h/.cpp`):** Spatial hash quantizing positions within $10^{-6}\text{ m}$, deduplicating vertices and pruning degenerate triangles.
- **Wavefront OBJ Importer (`src/io/ObjImport.h/.cpp`):** Reads `v` and `f`, supports fan triangulation, handles negative relative indices, skips comments and unneeded attributes, parses floats using locale-independent `std::from_chars`.
- **STL Importer (`src/io/StlImport.h/.cpp`):** Autodetects binary vs ASCII via 84 + $50 \times \text{count}$ formula. Welds duplicate vertices, generates outward normals.
- **OBJ Exporter (`src/io/ObjExport.h/.cpp`):** Outputs `o <name>`, `v x y z`, `f a b c` (1-based indices), locale-independent float formatting via `std::to_chars`.
- **UI Integration:**
  - `File -> Import...` with format detection, scale and Z-up conversion options dialog. Automatically creates object, assigns mesh, frames camera, and creates undo step.
  - `File -> Export OBJ...` writes active mesh.
  - `Object -> Assign Mesh Cube` converts active box to mesh.

---

## 3. Test Coverage

All tests automated with doctest in `Open_PhysX_tests.exe`:
- `tests/mesh_test.cpp`:
  - `T-B1-01`: Mesh validation detecting index mismatch, out-of-range indices, normal size mismatch.
  - `T-B1-02`: Unit length normal generation.
  - `T-B1-03`: Degenerate zero-area triangles producing no NaNs.
  - `T-B1-04`: 24-vertex cube with 6 outward-pointing face normals.
  - `T-B1-05`: Cube dimensions and bounds matching request.
- `tests/io_test.cpp`:
  - `T-B2-01 / T-B2-02`: Binary `Writer`/`Reader` LE round-trip and bounds overrun protections.
  - `T-B3-01 / T-B3-02`: Z-up to Y-up axis transformation math and determinant $+1$.
  - `T-B3-15 / T-B3-16 / T-B3-16b`: Extension and magic-byte sniffing (`BLENDER`, `glTF`).
  - `T-B3-03`: OBJ import of quad with negative indices and fan triangulation.
  - `T-B4-09`: OBJ export and re-import round-trip equality.

---

## 4. Next Priorities

1. **B2 (.opxs scene serialization):** Full scene chunks (`OBJS`, `MESH`) save and load.
2. **B5 (GLB import):** Vendor `cgltf`, implement `src/io/GlbImport.cpp`.
3. **C1 (Transform Gizmos):** Screen-space raycast handles for Move, Rotate, Scale.
