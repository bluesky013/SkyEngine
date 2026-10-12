## Context

A SkyEngine project is a directory with `assets/`, `configs/`, `cache/` and `<name>.skyproj`. Game logic is
shipped as a runtime **module**: a shared library exporting `StartModule(Environment*)`/`StopModule()` (macro
`REGISTER_MODULE(Name)` in `engine/framework/include/framework/interface/IModule.h`), loaded by `ModuleManager`
from a module list (`configs/modules_game.json`). Today `ProjectDescriptor::Create` only makes the directories and
the descriptor — there is no source tree or build entry for game code.

The engine build system (`sky_add_library`, `SKY_ENGINE_ROOT`, `3RD_PATH`, per-platform third-party bootstrap)
already produces these modules for engine/plugin code; the project build should reuse it rather than invent a
second build system.

## Goals / Non-Goals

**Goals**
- Creating a project generates a compilable Game module under `<Project>/source` plus a build entry.
- The generated project builds a SHARED module (named after the project) on every supported platform.
- Reuse the engine's CMake helpers and third-party flow; no new build system.
- The module is registered in `.skyproj` and the project module list so the launcher/editor loads it.

**Non-Goals**
- Shipping an installed engine SDK with `find_package` (future; see D1).
- Per-project editor plugin builds or asset cooking changes.
- iOS specifics (depends on `add-ios-platform`).

## Decisions

### D1. Engine reference: `add_subdirectory` first, SDK export later
> **Superseded by `add-project-native-layout`**, which replaces this with an independent CMake entry
> (`native/CMakeLists.txt` includes the engine `cmake/*` helpers and adds the engine + plugins targets).

Two ways for a project build to reach the engine:
- **A (initial)**: project `CMakeLists.txt` does `add_subdirectory("${SKY_ENGINE_ROOT}")` and then
  `add_subdirectory(source)`. Reuses the engine's targets/helpers directly; requires the engine source tree
  (path from a CMake preset or env `SKY_ENGINE_ROOT`). Simplest, no export work.
- **B (future)**: install/export engine targets and use `find_package(SkyEngine)` + a versioned SDK. Cleaner for
  distribution but needs install/export rules and an ABI/version story.

Start with A; leave the project CMake structured so B can replace the engine reference later.

### D2. Template as the single source of scaffolding
`engine/templates/game/` holds the scaffolded files with placeholder tokens (`{{PROJECT_NAME}}`, `{{MODULE_ID}}`).
The create flow copies the tree and substitutes tokens. Keeping templates out of code makes them editable and
reviewable.

> **Superseded by `add-project-native-layout`**: the scaffolded layout becomes `native/` + `build/<platform>/`
> instead of the root-level `source/` shown below.

Scaffolded layout:
```
<Project>/
  CMakeLists.txt              # add_subdirectory(engine) + source, project-level build entry
  CMakePresets.json           # per-platform configure/build (engine root, 3RD_PATH)
  <name>.skyproj              # modules += game module id
  assets/ configs/ cache/     # existing
  source/
    CMakeLists.txt            # sky_add_library(TARGET <Name>Module SHARED ...) via engine helpers
    include/<Name>Module.h
    src/<Name>Module.cpp      # class <Name>Module : public IModule + REGISTER_MODULE(<Name>Module)
    src/components/...        # one sample reflected component
  configs/modules_game.json   # engine modules + the game module
```

### D3. Module identity and naming
The game module target is `<ProjectName>Module` (SHARED). Its `StartModule`/`StopModule` are exported via
`REGISTER_MODULE`. The module id used in `.skyproj`/`modules_game.json` matches the target's runtime name
(the `DynamicModule` lookup name, following the existing convention).

### D4. Cross-platform
The project build inherits the engine's platform matrix and third-party flow: `3RD_PATH` per platform from
`python/third_party.py` (or an installed path), then CMake generate/build per platform. The game module links
engine interface/static targets the same way engine modules do; on Unix/macOS output is a `.dylib`/`.so`, on
Windows a `.dll`. iOS is deferred to `add-ios-platform`.

### D5. Where creation happens
> **Superseded by `add-project-native-layout`**, which makes the packaging CLI (`python/package_project.py create`)
> the single create implementation and has the editor shell out to it (removing `ProjectScaffold`).

The scaffold is applied by a `ProjectScaffold` helper invoked from `ProjectDescriptor::Create` (so non-editor
callers get it too). The editor Project Manager create action already routes through `ProjectDescriptor::Create`.

### D6. Reflection registration
The sample component shows the reflection/registration pattern so user components appear in the editor; this must
follow the engine's component registration rules (macro/registry at module `Start`).

### D7. Packaging ships cooked products (not source assets)
The asset pipeline separates **source assets** (authoring files under `assets/`) from **product assets** (cooked
outputs shipped as product bundles: `product.index` + `<uuid>.bin` payloads; see
`docs/features/asset-pipeline.md`). A shipped game package therefore contains the **cooked product bundles** for
the target platform, **not** the raw `assets/` sources:

- Packaging runs the asset cook for the project's sources with the platform's `CookConfig` target, producing that
  platform's product bundle; the bundle ships alongside the game module and engine bundles.
- The runtime loads product bundles (engine `common`/`tex_*` + the project's cooked bundle); it does not require
  `assets/` and has no source catalog.
- `--project <dir>` (this change's dev mode) reads the project tree including `assets/` and relies on on-demand
  cook in the editor — that is a development convenience, **not** the shipping layout.

### D8. Application identity, signing, and packaging presets

Packaging identity and options live in the project, not hardcoded:

- **Identity vs. presets vs. credentials** (Godot-style): `.skyproj` `settings` holds the default identity
  (`applicationId`/`versionName`/`versionCode`); `export_presets.json` (committed) holds per-platform package
  options (bundle id, variant, abis, keystore ref, team); `export_credentials.json` (not committed) holds secrets.
  Credentials resolve **env > file**. `python/package_project.py` reads both; the `.skyproj` schema stays defined in
  C++ (`ProjectDescriptor`) so the editor and runtime share one model.
- **Preset extra build options**: a preset may also carry `cacheVariables` (dict → CMake `-D`), `compileDefinitions`
  / `compileFlags` (→ `SKY_EXTRA_CXX_DEFINITIONS` / `SKY_EXTRA_CXX_FLAGS`), and `extraConfigureArgs` /
  `extraBuildArgs` (verbatim passthrough). Optional top-level `defaults` merge into every preset. Precedence:
  `defaults` < preset < env/CLI.
- **Packaging modes**: this change ships **dynamic-only** (each runtime module is a SHARED library with dynamic
  registration). Static bundling is a follow-up in `add-static-module-bundling`, which reuses the preset option
  surface above.

The per-platform project layout (where the generated IDE project lives and how the launcher consumes the resolved
bundle id/signing) is defined by `add-project-native-layout`. iOS packaging (IPA via `xcodebuild`) depends on
`add-ios-platform` and remains a follow-up.

## Risks / Trade-offs

- [Engine source coupling] -> the project consumes the engine source tree; D1-B (SDK) is the escape hatch.
- [Module naming collisions] -> enforce `<Name>Module`; validate the application id maps to a legal Java package.
- [Platform drift] -> the project entry includes the same `cmake/*.cmake` helpers; document the bootstrap.
- [Module ABI vs engine] -> the module must build with the same toolchain/CRT/`3RD_PATH` as the engine.
- [Heavy project build] -> `add_subdirectory(engine)+plugins` builds the whole engine per project. Mitigations:
  existing `SKY_BUILD_<plugin>` switches; a future installed-SDK mode (D1-B).
- [No engine-runnable host] -> removing `engine/launcher` means an engine-only build has no runnable exit; running
  is via `SandboxEditor` (editor) or a project build. Docs must reflect this.
- [Deploy anchor fragility] -> correctness depends on keeping the target name `Launcher` and calling
  `sky_set_dependency(TARGET Launcher)` once, after all engine/plugin/tool registrations.
- [Android output dir] -> overriding `CMAKE_*_OUTPUT_DIRECTORY` on Android breaks gradle's native-lib collection;
  the project entry must not do it when `ANDROID`.
- [Python runtime dependency] -> the editor shells out to the CLI for create/generate/package; needs Python (and the
  script) available, else a clear error.

## Open Questions

- D1-B installed SDK / export templates for distribution.
- Whether projects need their own third-party set beyond reusing the engine's `3RD_PATH`.
- Hot-reload of the game module (unload/reload) — separate feature?
