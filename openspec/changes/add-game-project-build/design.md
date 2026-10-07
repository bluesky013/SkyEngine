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

## Risks / Trade-offs

- [Engine source coupling] -> A couples the project to the engine source tree; D1-B (SDK) is the escape hatch.
- [Module naming collisions] -> enforce `<Name>Module` and reject names that collide with engine modules.
- [Platform drift] -> the project CMake mirrors engine presets; document the per-platform bootstrap.
- [Module ABI vs engine] -> the game module must build with the same toolchain/CRT/`3RD_PATH` as the engine; a
  mismatch is a load failure at runtime.
- [Large `add_subdirectory` cost] -> building the whole engine per project is heavy; gate with existing
  `SKY_BUILD_*` switches to build only needed parts.

## Open Questions

- SDK vs source-tree engine reference for distribution (D1-B design).
- Whether game projects need their own third-party set (beyond reusing the engine's `3RD_PATH`).
- Hot-reload of the game module (unload/reload) — separate feature?
