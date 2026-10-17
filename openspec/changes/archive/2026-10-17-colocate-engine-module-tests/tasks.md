## 1. Colocate core tests

- [x] 1.1 Move `engine/test/core/*` to `engine/core/test/*` without changing test contents.
- [x] 1.2 Add `engine/core/test/CMakeLists.txt` defining `CoreTest` with the same sources and libraries.
- [x] 1.3 Add `add_subdirectory(test)` to `engine/core/CMakeLists.txt` under `SKY_BUILD_TEST`.

## 2. Colocate framework tests and helpers

- [x] 2.1 Move `engine/test/framework/*` to `engine/framework/test/*`, preserving platform subdirectories and the Android source append.
- [x] 2.2 Move `engine/test/process_helper/*` and `engine/test/cook_worker_stub/*` under `engine/framework/test/`.
- [x] 2.3 Move the framework fixture assets and configs (`engine/test/assets`, `engine/test/configs`) under `engine/framework/test/`.
- [x] 2.4 Move `EngineRoot.h.in` under `engine/framework/test/`; generate it there and expose the generated include directory only to `FrameworkTest`, with `PROJECT_ROOT` pointing at the relocated fixtures.
- [x] 2.5 Add `engine/framework/test/CMakeLists.txt` defining `FrameworkTest`, `ProcessHelper`, and `CookWorkerStub`, preserving target names, libraries, working directory, schema definition, and iOS bundle settings.
- [x] 2.6 Preserve the helper dependency edges so both helpers build and deploy beside `FrameworkTest`.
- [x] 2.7 Add `add_subdirectory(test)` to `engine/framework/CMakeLists.txt` under `SKY_BUILD_TEST`.

## 3. Colocate UI tests

- [x] 3.1 Move `engine/test/ui/*.cpp` to `engine/ui/test/`, keeping each test in its existing target grouping.
- [x] 3.2 Add `engine/ui/test/CMakeLists.txt` defining `UICoreTest`, `UITextTest`, `UIEditorTest`, `UITextFreeTypeTest`, and `UIShaderTest` with existing libraries and conditions.
- [x] 3.3 Reference `engine/ui/render/test/UIShaderTest.cpp` and its include path from the UI test CMake without linking the UI runtime.
- [x] 3.4 Add `add_subdirectory(test)` to `engine/ui/CMakeLists.txt` under `SKY_BUILD_TEST`.

## 4. Remove the central aggregator and legacy tests

- [x] 4.1 Delete `engine/test/CMakeLists.txt`, `engine/test/main`-related shared entry files, and the now-empty `engine/test` tree after all owners configure their own tests.
- [x] 4.2 Delete `engine/test/render` and `engine/test/vulkan`.
- [x] 4.3 Remove the `SKY_BUILD_LEGACY_RENDER` test branch and any remaining references to `RenderTest`, `VulkanTest`, or `engine/test` from build files.
- [x] 4.4 Remove the empty `engine/test` directory and confirm no build file references it.

## 5. Remove the duplicate engine config tree

- [x] 5.1 Delete `engine/configs/mesh_build_presets.json` and the `engine/configs` directory.
- [x] 5.2 Confirm root `configs/mesh_build_presets.json` is unchanged and no CMake, deployment, packaging, or runtime path references `engine/configs`.

## 6. Update live references

- [x] 6.1 Update `python/tests/run_ios_platform_tests.py` paths to `engine/framework/test/platform/ios/...`.
- [x] 6.2 Update the active `openspec/changes/lazy-transform-update` test path reference.
- [x] 6.3 Update current documentation references to moved tests (for example `docs/editor/asset-browser.md`); leave archived OpenSpec changes untouched.

## 7. Verify and record

- [x] 7.1 Reconfigure the build so globbed source lists and removed directories are picked up.
- [x] 7.2 Build `CoreTest`, `FrameworkTest`, `UICoreTest`, `UITextTest`, `UIEditorTest`, `UIShaderTest`, `ProcessHelper`, and `CookWorkerStub`; build `UITextFreeTypeTest` where FreeType is enabled.
- [x] 7.3 Run the built test targets and confirm no regressions, including the process and out-of-process cook tests.
- [x] 7.4 Grep non-archived sources, scripts, and docs to confirm no stale `engine/test` or `engine/configs` references remain.
- [x] 7.5 Run `openspec validate colocate-engine-module-tests --strict --no-interactive`.
- [x] 7.6 Record verification results and any platform-specific validation left unrun in `tasks.md`.

## Verification record

- Reconfigured the macOS Ninja Release build (`build/`) with `SKY_BUILD_TEST=ON`, `SKY_BUILD_FREETYPE=ON`, `SKY_BUILD_LEGACY_RENDER=OFF`; configuration completed and the moved globs resolved.
- Built `CoreTest`, `FrameworkTest`, `ProcessHelper`, `CookWorkerStub`, `UICoreTest`, `UITextTest`, `UIEditorTest`, `UITextFreeTypeTest`, and `UIShaderTest`.
- Passed: `CoreTest` 276, `FrameworkTest` 183 (including the child-process and out-of-process cook tests), `UICoreTest` 185, `UITextTest` 25, `UIEditorTest` 3, `UITextFreeTypeTest` 2, `UIShaderTest` 6 passed with 3 DXIL cases skipped on macOS.
- Generated `build/Gen/test/EngineRoot.h` now reports `ENGINE_ROOT` at the repository root and `PROJECT_ROOT` at `engine/framework/test`; framework fixture lookups passed.
- Restored the test-mutated tracked `engine/framework/test/assets/assets.db` after the run.
- Confirmed `RenderTest`/`VulkanTest` are absent, `engine/test` and `engine/configs` are removed, and the root `configs/mesh_build_presets.json` is byte-identical to its pre-change content.
- Updated live references in `python/tests/run_ios_platform_tests.py`, `python/tests/test_export_preset_schema.py`, `configs/export_preset.schema.json`, `docs/editor/asset-browser.md`, `engine/aurora/AGENTS.md`, and the active `lazy-transform-update` proposal; archived OpenSpec records were not modified.
- `openspec validate colocate-engine-module-tests --strict --no-interactive` and `git diff --check` passed.
- Windows/VS 2022 and Android/iOS builds were not run on this macOS host; their build/packaging paths were updated but remain platform follow-ups.
