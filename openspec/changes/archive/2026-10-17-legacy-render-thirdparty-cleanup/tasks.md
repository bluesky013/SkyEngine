## 1. Delete legacy engine trees

- [x] 1.1 Delete `engine/render/**` (legacy RHI/backends, `RenderCore`, `RenderAdaptor`, builders, `SkyRender.Editor`).
- [x] 1.2 Delete `engine/shader/**` (legacy shader compiler, tool, and tests).
- [x] 1.3 Delete `engine/editor/**` (legacy Qt editor and `EditorFramework`).

## 2. Remove legacy build options and wiring

- [x] 2.1 Remove `SKY_BUILD_EDITOR` and `SKY_BUILD_LEGACY_RENDER` from `cmake/options.cmake`, including the `SKY_DEVELOP` and dependent conditions.
- [x] 2.2 Remove the legacy `shader`/`render` and legacy editor blocks from `engine/CMakeLists.txt`.
- [x] 2.3 Remove the trailing `sky_set_dependency(TARGET Editor)` block from the top-level `CMakeLists.txt`.
- [x] 2.4 Remove `SKY_BUILD_EDITOR` from `plugins/CMakeLists.txt` editor gating; keep `SKY_BUILD_SANDBOX` and `requires_editor` behavior.
- [x] 2.5 Update stale comments referring to the legacy Qt editor outside the deleted trees.

## 3. Delete legacy-only consumers

- [x] 3.1 Delete `plugins/xr/**` and remove the `xr` entry from `plugins/plugins.json`.
- [x] 3.2 Delete `plugins/pvs/runtime/render/**`, `plugins/pvs/runtime/Registry.cpp`, and `plugins/pvs/plugin.json`; reduce `plugins/pvs/CMakeLists.txt` to `PVS.Core` + `PVSTest`.
- [x] 3.3 Delete `tools/noise/**` and `tools/perf/**`; remove their commented entries from `tools/CMakeLists.txt`.
- [x] 3.4 Confirm `PVS.Core`/`PVSTest` no longer reference `RenderCore` or any legacy include, and that no target links `RHI`, `RenderCore`, `RenderAdaptor`, `VulkanRHI`, `SkyRender`, or `ShaderCompiler.Static`.

## 4. Remove legacy third-party dependencies

- [x] 4.1 Delete find modules: `Findboost.cmake`, `Findtaskflow.cmake`, `Findglslang.cmake`, `FindSPIRVCross.cmake`, `FindOpenXR.cmake`, `Findgles.cmake`, `Findimplot.cmake`.
- [x] 4.2 Delete unused find modules with no consumer: `Findktx.cmake`, `Findmimalloc.cmake`, `Findcxxopts.cmake`.
- [x] 4.3 Keep the `GKlib` and `metis` package entries and patches for future meshlet integration.
- [x] 4.4 Remove stale legacy comments/examples from `cmake/thirdparty.cmake` and `cmake/thirdparty_helpers.cmake`.
- [x] 4.5 Confirm no remaining `3rdParty::boost`, `taskflow`, `glslang`, `SPIRVCross`, `OpenXR`, `gles`, or `implot` references exist.

## 5. Update live documentation and skills

- [x] 5.1 Update `README.md` module table, options, and editor instructions to Aurora + sandbox only.
- [x] 5.2 Update `docs/plugins/pvs.md` to remove the legacy render glue section and describe only `PVS.Core` + `PVSTest`.
- [x] 5.3 Update `docs/aurora/shader-design-and-migration.md` to state the legacy trees are removed.
- [x] 5.4 Update the active `consolidate-active-changes` design so this change is no longer described as a blocked debt record.
- [x] 5.5 Update live build/architecture/project-reference skill copies under `.opencode/skills`, `.claude/skills`, and `.github/skills`: remove legacy targets, options, Qt5 requirements, and third-party entries.
- [x] 5.6 Leave archived OpenSpec changes untouched.

## 6. Verify

- [x] 6.1 Reconfigure the build and confirm it no longer searches for Qt5 or any removed third-party package.
- [x] 6.2 Build the full project and confirm Aurora, sandbox editor, PVS core, and remaining tools compile.
- [x] 6.3 Run the test suite; record any pre-existing/environmental failure separately.
- [x] 6.4 Grep non-archived sources, build files, scripts, docs, and skills for removed paths, targets, flags, and dependency names.
- [x] 6.5 Run `openspec validate legacy-render-thirdparty-cleanup --strict --no-interactive` and `git diff --check`.
- [x] 6.6 Record verification results and any unrun platform validation in `tasks.md`.

## Verification record

- Deleted `engine/render`, `engine/shader`, `engine/editor`, `plugins/xr`, `plugins/pvs/runtime/render`, `tools/noise`, and `tools/perf`.
- Removed `SKY_BUILD_LEGACY_RENDER`, `SKY_BUILD_EDITOR`, and the dead `SKY_BUILD_GLES` option; removed all legacy CMake branches and `find_package(Qt5)`.
- Reduced PVS to `PVS.Core` + `PVSTest`; no target links the removed RHI/render targets.
- Removed find modules for `boost`, `taskflow`, `glslang`, `SPIRV-Cross`, `OpenXR`, `gles`, `implot`, plus unused `ktx`, `mimalloc`, `cxxopts`. Kept `GKlib`/`metis` packages, find modules, and patches reserved for future meshlet integration.
- Reconfigured the macOS Ninja Release build; configuration completed without probing Qt5 or any removed third-party package.
- Built the full project successfully, including Aurora, `SandboxEditor`, `PVS.Core`, and remaining tools.
- Test suite: 36/37 passed. `EnetNetworkTest.ConformanceOverUdp` still fails with the same connection-timeout failure observed before this change; no other test regressed.
- Restored the test-mutated `engine/framework/test/assets/assets.db`.
- Remaining scans only match unrelated identifiers (`editor_window.json`, vegetation's `VegetationRenderAdaptor`) and historical notes describing the former targets.
- Updated current documentation and all live skill copies; archived OpenSpec records were not modified.
- Windows/VS 2022, Android, and iOS builds were not run on this macOS host and remain platform follow-ups.
- Follow-up before commit: kept `GKlib`/`metis` find modules, package entries, and patches for future meshlet integration; removed `cxxopts` entirely. Reconfigure and full build still pass with no legacy targets.
