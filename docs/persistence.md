# Persistence (project files, autosave, recovery)

How OpenPhysX keeps a project on disk, never loses it, and repairs it when a file is damaged. This page tracks `src/persistence/` and `src/ui/file_menu.cpp`. The data model lives in [architecture.md](architecture.md); this page is about the bytes.

## Project file layout

```
MyRobot.opx                 <- main project file (JSON, human-readable, ~1 line per field)
MyRobot.opx.data/           <- sidecar directory, created only if needed
    assets/                 <- imported meshes, textures, copied in on import
    cache/                  <- simulation results and CFD fields (regenerable)
```

- Large binary data is **never inlined** in JSON. The main file references it by relative path plus an XXH64 content hash.
- Only **relative paths** inside the file, so the project folder can be moved as a unit.
- The `.opx.data` dir is written **before** the main file, so the main file never points at data that is missing.

## Top-level schema (format v1)

```json
{
  "magic": "OPX",
  "format_version": 1,
  "app_version": "0.1.0",
  "created_utc": "...",
  "modified_utc": "...",
  "checksum": "xxh64:....",
  "units": { "length": "m", "mass": "kg", "time": "s", "angle": "rad" },
  "scene": { "entities": [ { "id": "hex", "components": { "Tag": {...}, ... } } ] },
  "simulation": { "gravity": [...], "timestep": 0.001, ... },
  "editor": { "camera": {...}, "selection": [...], "layout_ini": "..." },
  "assets": [ { "id": "...", "path": "assets/arm.obj", "hash": "xxh64:..." } ]
}
```

| Section | Saved | Never saved (runtime only) |
| --- | --- | --- |
| `scene.entities` | Components with `serializable = true` in the component registry | GPU handles, solver indexes, contact caches, `entt::entity` values |
| `scene` hierarchy | `RelationshipComponent` (parent/children UUIDs) | Resolved entity caches |
| Rigid body / collider / joint | Mass, inertia, shapes, materials, limits, motors | Broadphase and solver state |
| Simulation | Gravity, timestep, loop, demo motion, colors | Playback time, the runtime (play-mode) scene |
| Editor | Camera, selection, ImGui layout string | Undo stack |
| Assets | Table of id / path / hash | The sidecar bytes |
| CFD | Domain bounds, resolution, boundary conditions | Result fields (go to `cache/`) |

## Writing and reading

- **Identity is UUID, not `entt::entity`.** Entities are written in creation order (a monotonic counter in `Scene::CreateEntity`). JSON object keys are sorted by `nlohmann::json` with `dump(2)`, so saving the same project twice produces byte-identical files apart from `modified_utc` (`payload_checksum` and the tests exclude it from comparison).
- **Floats use `nlohmann::json`'s shortest round-trip form** (via `std::to_chars` for doubles). `float` → `double` → `float` is exact for every test value used, including `-0.0`, `FLT_MIN`, `FLT_MAX`, and `FLT_EPSILON`.
- **Backwards and forwards compatible:** unknown fields are ignored, missing fields get component defaults (every `from_json` uses `j.value("field", default)`), and unknown component names are kept verbatim and written back, so newer files opened in this version do not silently lose data. Unknown names and dangling references land in the `LoadReport` as warnings, not errors.
- **Errors are `std::expected`, never exceptions across module boundaries.** `ProjectFile::Load` returns `std::expected<LoadReport, LoadError>`; `Save` returns `std::expected<void, SaveError>`. UI modals show the message and never let a corrupt load replace the current project ("load into a temporary `Project`, swap on success").

## Atomic save (the critical path)

`AtomicWrite` never writes to the target directly. It does, in order:

1. `ProjectFile::Serialize` builds the full JSON in memory. A serialization failure leaves the existing file untouched.
2. The XXH64 checksum is computed over the whole document except `checksum` and `modified_utc`, and embedded.
3. Sidecar files (assets/cache) are written first. Main file last.
4. Content is written to `<target>.tmp` **in the same directory** (same filesystem, so rename is atomic).
5. Flush + fsync (`FlushFileBuffers` on Windows).
6. Any existing `<target>` is moved to `<target>.bak`, replacing an old `.bak`.
7. `.tmp` is atomically renamed over the target (`MoveFileExW ... MOVEFILE_REPLACE_EXISTING` on Windows, `rename` on POSIX).
8. On failure the original file and its `.bak` are intact; the `.tmp` is deleted and the error reported.

On load the checksum is verified. A mismatch refuses the file with "This file may be corrupt" — and the message mentions the `.bak` when one exists, so the user can open that instead. `tests/project_file.cpp` injects a failure between the tmp write and the rename and asserts the original is untouched.

## Validation on load

Everything read from a file is untrusted. After entities and components are read, a `validate_scene` pass runs over the loaded scene, and `ProjectFile::Load` checks the simulation block. Checks and their safe defaults:

| Check | Default used | Note |
| --- | --- | --- |
| NaN / Inf in transform, box, material, body, collider, joint, CFD values | `0` / `1` / identity / component default | Non-finite numbers cannot actually be serialized to JSON, but a repaired hand-edited file can contain them. |
| Invalid or zero-length quaternion | identity | A zero quaternion would poison every transform multiply. |
| `mass <= 0` | `1.0` | **Only for dynamic bodies.** Static/kinematic bodies may carry a zero mass from an exporter and keep it — re-saving must stay byte identical. |
| Zero-size shapes (box half extents, sphere/capsule radius, capsule/cylinder height, primitive box size) | positive default per shape | A box collider's `height`/`radius` are not validated, and vice-versa. |
| `timestep <= 0` or non-finite | `0.001` | |
| Non-finite gravity | `{0, -9.81, 0}` | Defensive only; strict JSON cannot carry infinity. |
| CFD resolution `<= 0`, density/viscosity `<= 0` | `32` / `1.2` / `1.8e-5` | |
| Missing entity IDs, duplicate IDs, dangling references, hierarchy cycles | skip / clear field / break cycle | Already handled by `scene_serializer.cpp`; warnings only. |
| Missing asset file | placeholder, never a crash | Warning in the report. |

Every repair produces a **warning in the `LoadReport`**, and the repaired values are what is saved back — the user fixes a damaged file just by saving it. The UI surfaces the report in the **Load Report** panel (Window menu, auto-opens after a load with entries): severity-colored rows, then a Clear button.

## Dirty tracking

Dirty state is **not a flag** — it is `commandStack.revision() != savedRevision`. Every user edit goes through a `Command` on the stack; save sets `savedRevision`. Undoing back past a save makes the file dirty again; redoing back to the save point makes it clean. Selection, camera movement, and simulation stepping never touch the revision. See `src/editor/command.hpp` and `src/persistence/project_manager.hpp`.

## Autosave

- Interval configurable, default 2 minutes; only runs when the project **is dirty and the user is idle** (no drag, no text edit, no open modal).
- Location is **never the project folder**:
  `<appdata>/OpenPhysX/autosave/<session-id>/<projectname>_<timestamp>.opx`
  Untitled projects autosave as `Untitled_<timestamp>.opx`.
- The snapshot is built on the main thread; serialization happens on a background `std::jthread` that touches only the snapshot, never the live registry, OpenGL, or raylib. No frame hitches.
- Uses `AtomicWrite` like everything else. Keeps the last 5 autosaves per session, deletes older ones, and deletes the session's autosaves on clean exit and after a successful manual save.
- Autosave does **not** clear the dirty flag or move `savedRevision`. A small **"Autosaved"** indicator appears in the viewport for a few seconds.

## Crash recovery

1. On startup the app creates `<autosave>/<session-id>/session.lock` (PID + start time) and installs a best-effort crash handler (`std::set_terminate`, `signal(SIGSEGV)`/`SIGABRT`) that only writes a tiny crash-marker file — no serialization inside a signal handler.
2. Clean shutdown removes the lock and the marker.
3. On the next launch, `FindCrashedSessions` scans the autosave directory. A session folder whose lock points at a dead PID, or a leftover folder with autosaves and no clean-exit marker, is a crashed session.
4. A modal offers **Recover / Discard / Decide Later** for the newest crash, newest first when several exist. Recovery opens the autosave as an **untitled, dirty** project (the original path is kept as a hint for Save As). The original file is never silently replaced.
5. Per-session folders mean two app instances can run concurrently without fighting.

## Migrations

- `format_version` is an integer bumped on every breaking schema change; components carry their own per-component versions.
- `MigrateProject` is a chain (`v1→v2`, `v2→v3`, …) that operates on the **JSON tree before** deserialization.
- A file newer than the app supports is refused with a clear "created by a newer version of OpenPhysX" message — never silently dropped and overwritten.
- When a migrated file is saved for the first time, the original is archived as `<name>.v<old>.bak` (`ArchiveMigratedOriginal`).
- Golden files that must always load: `tests/data/v1/cube.opx`.

## Robustness handled

- Unicode paths (`fs::path` + `.wstring()` on Windows; tested with a `café.opx`), read-only/disk-full/indexing errors, files larger than 64 MB refused, JSON nesting deeper than 64 refused before recursion can hurt, absurd nesting tests in the suite.
- The loader refuses: empty file, non-JSON, wrong magic, bad checksum, missing scene, too-new version, truncated content, out-of-range numbers (nlohmann rejects `1e999`). The current project stays loaded on every failure.

## Known limitations (v0.1.0)

- No "pack to a single zip" export yet — the sidecar design is chosen so it can be added later without changing the schema.
- `modified_utc` changes on every save, so the checksum excludes it (a save is byte-identical only modulo the timestamp).
- The single `.bak` is the only second chance for the main file; if both main and `.bak` are corrupt the file is gone. Autosave is the real safety net.
- Validation is a growable list inside `scene_serializer.cpp`; new components need their own `from_json` defaults but only explicitly added checks join the pass.