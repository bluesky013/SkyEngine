## Context

`add-game-project-build` scaffolded a project at the repository-style root (`CMakeLists.txt`, `source/`) and the
engine ships a runnable `Launcher` whose CMake also deploys configs/templates. The project build adds the engine's
**root** project (`add_subdirectory(${SKY_ENGINE_ROOT})`), which pulls in engine dev glue (`PROJECT(SkyEngine)`,
global standard, `output/bin` override, `tools/`, `sky_set_dependency(Launcher/Editor)`). This duplicates the
launcher, ties generated IDE projects to the engine tree, and fragments desktop outputs.

This change restructures a project to own one native codebase (`native/`) and build per-platform project/IDE outputs
under `build/<platform>/`, consuming the engine as a library through an independent CMake entry.

## Goals / Non-Goals

**Goals:**
- One `native/` codebase (game module + platform launcher entry + build entry) per project; per-platform build
  projects/artifacts under `build/<platform>/`.
- Consume the engine without running its root CMake project.
- Single runnable host per project; engine no longer ships a launcher.
- Consistent artifact naming (`<Name>`) across platforms.

**Non-Goals:**
- Installed engine SDK / `find_package` (future; `add-game-project-build` D1-B).
- Static module bundling (`add-static-module-bundling`).
- Project-local plugins.
- Hot reload.

## Decisions

### D1. Independent CMake entry (not the engine root project)
The project's `native/CMakeLists.txt` consumes the engine as a library:

```cmake
project(<Name>)
# set C++20, ENGINE_ROOT=SKY_ENGINE_ROOT, per-platform output dir (NOT on Android)
include(${ENGINE_ROOT}/cmake/{functions,options,plugin_switches,configuration,thirdparty}.cmake)
add_subdirectory(${ENGINE_ROOT}/engine  engine)
add_subdirectory(${ENGINE_ROOT}/plugins plugins)
# platform launcher + native/source game module, then:
sky_set_dependency(TARGET Launcher)   # binds every DEPENDENCIES Launcher registration
```

This avoids re-running the engine root CMakeLists. Verified: the engine `cmake/*.cmake` helpers reference only
`${ENGINE_ROOT}` (no `CMAKE_SOURCE_DIR`), and `plugins/CMakeLists.txt` is self-contained.
`plugin_switches.cmake` must be included (defines `SKY_BUILD_<x>`).

### D2. `native/` layout
```
<Project>/
  <name>.skyproj  export_presets.json  .gitignore
  native/
    CMakeLists.txt              # independent engine-consuming entry (D1)
    CMakePresets.json
    source/                     # game module <Name>Module
    launcher/ or <platform>/    # platform launcher entry (D3)
  assets/ configs/ cache/
  build/<platform>/             # generated build/IDE projects + artifacts (D5)
```

### D3. Project-owned launcher; engine launcher removed
The engine no longer defines a runnable launcher. The **target name stays `Launcher`** so all
`DEPENDENCIES Launcher` registrations (engine, `plugins/*/plugin.json`, `tools/`) keep working through the deferred
`sky_set_dependency` binding; the **project defines it** in `native/`. Platform entry sources are templates
(`engine/templates/launcher/{android,ios,macos,windows}`) copied into `native/<platform>/` at create time; Android
Java/resources/manifest are **copied and tokenized by application id** (project owns them).

- Remove `engine/launcher/**`; drop `add_subdirectory(launcher)` + `sky_set_dependency(TARGET Launcher)` +
  `if (TARGET Launcher)` glue (`engine/CMakeLists.txt:24-25,42-44`) and `sky_set_dependency(TARGET Launcher)` in the
  root `CMakeLists.txt`.
- The project launcher owns deployment of `configs/` (engine first, then project so `modules_game.json` overrides),
  `templates/`, and the game module. `SandboxEditor` already deploys its own resources/configs/templates.

### D4. Artifact naming = project identifier
`OUTPUT_NAME "<Name>"` on every platform (`<Name>.exe` / `<Name>` / `<Name>.app` / `lib<Name>.so`), independent of
the target name; iOS via `XCODE_ATTRIBUTE_PRODUCT_NAME`; macOS `.app` is configurable via the export preset. For
Android, `SkyEngineActivity.loadLibrary("<Name>")` and the manifest `android.app.lib_name="<Name>"` are tokenized
(`{{LAUNCHER_LIB_NAME}}`) and must stay in lock-step. Display name (app label / `CFBundleDisplayName`) is separate
(`{{PROJECT_DISPLAY_NAME}}`).

### D5. Output directories are per-platform-correct
Android must **not** override `CMAKE_*_OUTPUT_DIRECTORY` (gradle's `externalNativeBuild` collects the `.so`).
Desktop sets a single `<Project>/build/<platform>/bin` (multi-config generators include the `$<CONFIG>` subdir to
match `cmake/functions.cmake`'s deploy path). Generated IDE/build projects live under `<Project>/build/<platform>/`.

### D5b. Preset packaging options (signing, bundle, identity)
The export preset carries the per-platform packaging options and the CLI translates them into configure args:

- `bundleId` (defaults to the project application id), `versionName`/`versionCode`.
- **Apple**: `signing` (`auto` | `off`); `bundle` (macOS `.app`); `team` / `signingIdentity` / `provisioningProfile`
  (override `export_credentials.json`). Mapped to `SKY_BUNDLE_ID`, `SKY_MACOS_BUNDLE`, `SKY_APPLE_*`; `signing: off`
  disables Xcode code signing (`CODE_SIGNING_ALLOWED=NO`) so CI can build unsigned.
- **Android**: `variant`, `abis`, `keystore`, `compileSdk`/`minSdk`/`targetSdk`/`ndkVersion`.
- **Extra build options**: `cacheVariables`, `compileDefinitions`/`compileFlags`
  (`SKY_EXTRA_CXX_DEFINITIONS`/`SKY_EXTRA_CXX_FLAGS`, applied by the project entry), `extraConfigureArgs`/`extraBuildArgs`.
  Optional top-level `defaults` merge into every preset. Precedence: `defaults` < preset < env/CLI.

These options are edited through the **editor Project Manager using the generic reflection panel** (AGENTS rule):
the preset data object is reflection-registered in the core/plugin side and the view binds it with
`ReflectedFormView::Bind(...)` (edits via `CommandService`); no hand-crafted per-field form. See task §7.

### D6. CLI-authoritative creation
`python/package_project.py create` is the single create implementation (full token set, launcher templates, package
token); the editor Project Manager shells out to it (like Unreal's editor invoking UAT) and fails with a clear
message when Python is unavailable. `ProjectScaffold` (C++) is removed.

### D7. Plugins and configs
Engine plugins are built by `add_subdirectory(${ENGINE_ROOT}/plugins)`, gated by `SKY_BUILD_<x>` (defaults from
`plugins/plugins.json`, overridable per preset via `cacheVariables`); editor-only plugins are skipped in a project
build. Plugin modules land in the unified output dir and build alongside `Launcher` via their `DEPENDENCIES Launcher`
registration. **Project-local plugins are out of scope** (game code lives in `native/source`). Compile-time selection
(`SKY_BUILD_<x>`) and the runtime module list (`configs/modules_game.json`) are independent and must stay in sync;
the project's `modules_game.json` must list the platform's runtime module set.

## Risks / Trade-offs

- [No engine-runnable host] -> an engine-only build has no runnable exit; running is via `SandboxEditor` or a project
  build. Docs must change.
- [Deploy anchor fragility] -> correctness depends on keeping the target name `Launcher` and calling
  `sky_set_dependency(TARGET Launcher)` exactly once, after all registrations.
- [Android output dir] -> overriding output dirs on Android breaks gradle's native-lib collection (D5).
- [Naming lock-step] -> Android lib name must match `loadLibrary` + manifest `lib_name` (tokenized).
- [Package-token legality] -> `<Name>` must be a legal identifier; the application id must map to a legal Java
  package.
- [Heavy build] -> `add_subdirectory(engine)+plugins` builds the engine per project; mitigate with `SKY_BUILD_<x>` and
  a future SDK mode.
- [Python runtime dependency] -> editor create/generate/package needs Python + the script available.

## Migration Plan

- **Prerequisite**: this change MODIFIES requirements of `game-project-build` and `editor-project-manager`, so
  `add-game-project-build` SHOULD be archived before (or together with) this one; otherwise the MODIFIED deltas have
  no base spec to apply to.
- No released projects (`*.skyproj`) exist yet, so the layout change is applied in place.
- Engine-only dev builds lose their runnable launcher; use `SandboxEditor` or build a project.
- Rollback: revert the engine-entry/template changes.

## Open Questions

- Windows launcher entry: keep `WIN32` subsystem, and whether a Linux host is in scope (engine has no Linux launcher).
- Where the deployed editor finds the CLI script (repo path / `SKY_PYTHON` / bundled with the editor).
