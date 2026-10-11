## Why

The project build currently adds the engine's **root** CMake project (`add_subdirectory(${SKY_ENGINE_ROOT})`), which
pulls in engine dev glue (global standard, `output/bin` override, `tools/`) and the engine's own runnable
`Launcher`. The launcher is then duplicated (engine + project), generated IDE projects are tied to the engine tree,
and desktop outputs fragment between `engine/output/bin` and the project. A project should instead own **one native
codebase** and produce per-platform build projects from a clean engine-library entry.

## What Changes

- Introduce `native/` as the project's single native codebase: the game module, the **platform launcher entry**, and
  an **independent CMake entry** that consumes the engine as a library (include the engine `cmake/*` helpers and add
  the `engine` + `plugins` targets - **not** the engine root project).
- **BREAKING**: the engine no longer ships a runnable launcher. The launcher **target name stays `Launcher`** (so
  every `DEPENDENCIES Launcher` registration keeps working), but it is **defined by the project**. An engine-only
  build has no runnable host (run via `SandboxEditor` or a project build).
- Artifact name = the project identifier `<Name>` on every platform (`<Name>.exe`/`<Name>.app`/`lib<Name>.so`),
  independent of the target name; the Android Java activity + manifest are tokenized for the library name.
- Generated build/IDE projects and artifacts go under `<Project>/build/<platform>/`; output directories are
  per-platform-correct (Android must not override `CMAKE_*_OUTPUT_DIRECTORY`).
- Platform launcher entries come from engine templates (`engine/templates/launcher/{android,ios,macos,windows}`),
  with Android Java/resources **copied and tokenized by application id** into `native/`.
- Project creation is **CLI-authoritative** (`python/package_project.py create`); the editor shells out to it.
- Plugin handling: engine plugins are built via `add_subdirectory(${ENGINE_ROOT}/plugins)` gated by `SKY_BUILD_<x>`;
  project-local plugins are out of scope. The project launcher deploys engine configs then project configs.

## Capabilities

### New Capabilities
- `project-native-layout`: the project's `native/` layout, independent engine CMake entry, project-owned launcher
  (engine launcher removal), artifact naming, `build/<platform>` outputs, and CLI-authoritative creation.

### Modified Capabilities
- `game-project-build`: the project scaffolding requirement (now `native/`) and the engine-reference requirement (now an independent CMake entry).
- `editor-project-manager`: the project-descriptor requirement (creation scaffolds `native/` via the CLI).
- `aurora-launcher`: the "launcher loads only AuroraRender" requirement drops the engine-`Launcher` link clause; the `Launcher --app editor` requirement is removed.
- `editor-application`: the editor host is `SandboxEditor` only (no `--app editor`).

## Impact

- Engine CMake: remove `engine/launcher/**`; drop `add_subdirectory(launcher)` + `sky_set_dependency(TARGET Launcher)`
  glue in `engine/CMakeLists.txt` / root `CMakeLists.txt`; retire `cmake/template/*`.
- Project templates: `engine/templates/{game,launcher}/`; restructure `game` to `native/`; `templates/ide/android`
  references `native/`.
- `python/package_project.py` (create/generate/package, `build/<platform>` outputs, launcher templates).
- Docs/skills/README/AGENTS and `add-ios-platform` design (engine launcher -> project launcher template).
