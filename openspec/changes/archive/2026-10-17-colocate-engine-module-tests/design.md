## Context

`engine/test` is the last central test aggregator for engine foundations. It owns:

- `core` and `ui` test sources compiled by `CoreTest` and five UI targets.
- `framework` tests plus two child-process helpers (`ProcessHelper`, `CookWorkerStub`), fixture assets/configs, and a generated `test/EngineRoot.h`.
- `render` and `vulkan` tests that only build with the deprecated `SKY_BUILD_LEGACY_RENDER` opt-in.

Aurora (`engine/aurora/*`), shader (`engine/shader/test`), and plugins already configure their own tests next to the code under `SKY_BUILD_TEST`. The aggregator therefore needs cross-module source paths such as `${ENGINE_ROOT}/engine/framework/platform/android/AndroidInput.cpp`, and it keeps test entries for a stack the default build does not compile.

Separately, `engine/configs/` contains a tracked `mesh_build_presets.json` that is byte-identical to `configs/mesh_build_presets.json` and is not read by CMake, deployment, or packaging. Both files were introduced in the same commit; the root directory is the one actually consumed.

## Goals / Non-Goals

**Goals:**

- Make every engine module own its tests, helpers, and fixtures, matching the Aurora/plugin convention.
- Preserve test target names so existing commands and scripts keep working.
- Remove stale legacy render test targets.
- Establish root `configs/` as the single engine builtin configuration location.
- Update current references without rewriting archived OpenSpec records.

**Non-Goals:**

- No test behavior, coverage, or assertion changes beyond path setup.
- No engine API or runtime behavior changes.
- No removal of the legacy `engine/render`/`engine/shader` source stack; its full retirement and third-party cleanup remain tracked by `legacy-render-thirdparty-cleanup`.
- No change to configuration content, deployment precedence, or project override behavior.
- No new third-party dependencies.

## Decisions

### 1. Move tests into `<module>/test` and configure from the module CMake

Target layout after the change:

| Owner | Test directory | Targets |
|---|---|---|
| `engine/core` | `engine/core/test` | `CoreTest` |
| `engine/framework` | `engine/framework/test` | `FrameworkTest`, `ProcessHelper`, `CookWorkerStub` |
| `engine/ui` | `engine/ui/test` | `UICoreTest`, `UITextTest`, `UITextFreeTypeTest`, `UIEditorTest`, `UIShaderTest` |

Each owner's CMake adds `test` only when `SKY_BUILD_TEST` is enabled. This mirrors `engine/aurora/*/CMakeLists.txt` and `plugins/*/CMakeLists.txt`.

Alternative considered: keep the aggregator but forward each subdirectory. Rejected because it preserves cross-module path ownership and keeps legacy entries in a central file.

### 2. Keep target names and working directories

`sky_add_test` already registers tests by target name, so names remain the compatibility contract. Test working directories stay `${CMAKE_SOURCE_DIR}` (repository root), preserving fixture lookups and schema paths.

### 3. Move `EngineRoot.h` generation with FrameworkTest

Only `AssetManagerTest` includes `<test/EngineRoot.h>`. Generate it from a template in `engine/framework/test` and expose its output directory as a target include, not a directory-wide include. Define `PROJECT_ROOT` from that template's own source directory so it points at the relocated framework fixtures.

### 4. Keep the UI shader contract source with the renderer sources

`engine/ui/render/test/UIShaderTest.cpp` already sits with the UI renderer. The `UIShaderTest` target moves to `engine/ui/test/CMakeLists.txt` and references that file inside the same module; it does not compile UI runtime sources, so it stays a standalone contract test.

### 5. Remove only legacy test entries, not the legacy stack

Delete `engine/test/render` and `engine/test/vulkan`. Drop the `SKY_BUILD_LEGACY_RENDER` branch from the test configuration. Leave `engine/render`, `engine/shader`, `plugins/pvs`' gated render glue, and the opt-in option untouched so the retirement work stays in its existing tracked change.

### 6. Delete `engine/configs/`

Remove the directory; root `configs/` is already used by:

- top-level CMake for `modules_editor.json`
- SandboxEditor deployment (`${CMAKE_SOURCE_DIR}/configs`)
- generated game deployment (`${ENGINE_ROOT}/configs`, then project `configs`)
- Python packaging (`REPO_ROOT/configs`, then project `configs`)

Alternative considered: keep `engine/configs` as the canonical location and update all consumers. Rejected because it would churn build/packaging paths and invalidate documented layout for no benefit.

### 7. Update only live references

Update `python/tests/run_ios_platform_tests.py`, the active `lazy-transform-update` proposal path, and current docs that cite `engine/test/...`. Archived OpenSpec changes and verification records are historical and remain unchanged.

## Risks / Trade-offs

- **[CMake globs do not notice moves until reconfigure]** → Re-run configure after moving files; the existing build command guidance already requires it for added/removed sources.
- **[Fixture paths silently break]** → Keep repository-root working directories and verify `AssetManagerTest` after relocating `PROJECT_ROOT`.
- **[Helper executables are not found at runtime]** → Preserve the dependency edges so `ProcessHelper` and `CookWorkerStub` deploy beside `FrameworkTest`, and run the process/cook tests.
- **[Android/iOS test entry points reference the old paths]** → Update the iOS Python runner and keep platform source globs relative to `engine/framework/test`; verify the Android headless entry still excludes `main.cpp` as before.
- **[Tracked `assets.db` is mutated by tests]** → Preserve the fixture outside generated product output and record any post-run cleanup already required by the test suite.
- **[Docs/active-change drift]** → Grep non-archived Markdown for `engine/test` and update only live references.

## Migration Plan

1. Add `engine/core/test`, `engine/framework/test`, and `engine/ui/test`; move sources, fixtures, helpers, and the `EngineRoot.h` template; wire `add_subdirectory(test)` into each module.
2. Update helper dependency edges and platform-specific source/include handling.
3. Delete `engine/test` and `engine/configs`.
4. Update live references in scripts, active OpenSpec artifacts, and docs.
5. Reconfigure, build the preserved test targets, run them, and run strict OpenSpec validation.

Rollback is a straight revert of the move: the target names and file contents are unchanged, so restoring `engine/test` and its CMake entry recovers the previous layout.

## Open Questions

None. Full legacy stack retirement remains out of scope and already has a tracked debt change.
