## Why

Today every runtime module is built as a **SHARED** library that self-registers at load time (`REGISTER_MODULE`
defining `StartModule`/`StopModule`), so a shipped game carries a directory of `.so`/`.dll` files. The intended
convention is a module **static** core plus a **dynamic** registration shim (the shim only wires the module into the
registry). The repo does **not** fully realize this split yet - this change both establishes/verifies it and adds the
packaging shape it enables: bundle selected modules **statically** into the executable/bundle and keep only the
dynamic registration behavior. This lets a project trade load time, file count, and symbol visibility against
runtime flexibility, driven by build/packaging options.

## What Changes

- Define two module packaging modes: **dynamic** (default; one shared library per module) and **static bundling**
  (selected modules linked into the host/bundle per rules).
- Add build/packaging options to select the mode and the bundling rules (which modules are static, order/grouping),
  surfaced through the project export preset.
- Static bundling links the modules' **static** parts into the host and keeps only the **dynamic** registration
  logic where dynamic loading is still required.
- The current change (`add-game-project-build`) ships **dynamic-only**; this change adds the static path.

## Capabilities

### New Capabilities
- `module-packaging`: module packaging modes (all-dynamic vs static bundling) and the build/packaging options +
  rules that control them.

### Modified Capabilities
<!-- No existing spec requirements change; the dynamic path stays the default. -->

## Impact

- Engine build system: module targets (`engine/CMakeLists.txt`, per-module `CMakeLists.txt`), a static/dynamic split
  convention, and packaging-option plumbing (`cmake/*.cmake`).
- Project build/packaging: `python/package_project.py`, `export_presets.json` schema, and the project's
  `native/CMakeLists.txt` (defined by `add-project-native-layout`, a prerequisite).
- Runtime: `ModuleManager` / launcher must accept statically-provided modules alongside dynamically loaded ones.
- Docs: `docs/features/game-project-build.md`, module packaging guide.
