## Why

Engine tests are centralized under `engine/test/{core,framework,ui,render,vulkan}`, while Aurora, shader, and plugin tests already live next to the code they exercise. The central aggregator now owns cross-module paths (`${ENGINE_ROOT}/engine/framework/...`), stale targets for the opt-in legacy render stack, and two child-process helpers that only `FrameworkTest` uses. Repository configuration is similarly ambiguous: `engine/configs/` contains one tracked file that is byte-identical to `configs/mesh_build_presets.json` and is referenced by no build or packaging path.

## What Changes

- Move `CoreTest` into `engine/core/test`, `FrameworkTest` plus its `ProcessHelper`/`CookWorkerStub` helpers and fixtures into `engine/framework/test`, and the UI test targets into `engine/ui/test`; configure each from its owning module under `SKY_BUILD_TEST`.
- Remove the central `engine/test` aggregator, its duplicated test entry points, and the per-module path reach-ins it required.
- Keep existing test target names (`CoreTest`, `FrameworkTest`, `UICoreTest`, `UITextTest`, `UITextFreeTypeTest`, `UIEditorTest`, `UIShaderTest`, `ProcessHelper`, `CookWorkerStub`) so scripts and documented commands continue to work.
- Delete `RenderTest` and `VulkanTest`; they only exercise the deprecated legacy `RenderCore`/`VulkanRHI` stack. Retiring the legacy stack itself remains tracked by `legacy-render-thirdparty-cleanup`.
- Delete the duplicate `engine/configs/` directory. The repository-root `configs/` remains the single source for engine builtin configuration used by CMake, editor/game deployment, and Python packaging.
- Update non-archived references to the moved paths: the iOS test runner, OpenSpec's active change notes, and current documentation. Archived OpenSpec changes remain historical records and are not rewritten.

## Capabilities

### New Capabilities

- `engine-test-layout`: tests, helpers, and fixtures live inside the module they exercise and are configured by that module under `SKY_BUILD_TEST`.
- `engine-config-layout`: engine builtin configuration has one authoritative repository location (`configs/`), with no duplicate `engine/configs/` tree.

### Modified Capabilities

None. Test target names, engine runtime behavior, asset formats, and deployment precedence are unchanged.

## Impact

- Build files: `engine/core`, `engine/framework`, and `engine/ui` gain test subdirectories; `engine/test` is removed; legacy test entries disappear.
- Test code and fixtures: moved without behavior changes; `EngineRoot.h` generation and fixture-root definitions move with `FrameworkTest`.
- Repository layout: `engine/configs/` is removed; root `configs/` contents are unchanged.
- External references: `python/tests/run_ios_platform_tests.py`, one active OpenSpec change, and current docs are updated; archived change records are untouched.
- No engine API, runtime behavior, or third-party dependency changes.
