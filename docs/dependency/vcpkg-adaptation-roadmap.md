---
title: "SkyEngine Third-Party Package Adaptation to vcpkg + Custom Registry"
description: "Complete adaptation inventory, tasks, constraints, and phased roadmap for SkyEngine third-party dependencies."
module: "build-system"
updated: "2026-03-29"
---

## Scope And Sources

This document is based on the current dependency definitions and build wiring in:
- CMake manifest and feature mapping in `CMakeLists.txt` and `cmake/vcpkg.cmake`
- Dependency declarations in `vcpkg.json` and `vcpkg-configuration.json`
- Legacy third-party source list in `cmake/thirdparty.json`
- Plugin switches in `plugins/plugins.json`
- Attached custom registry in `vcpkg-registry/`

The goal is to manage third-party packages consistently through vcpkg, while using a custom registry for ports that are missing or require project-specific behavior.

## 0) Package Classification: Adaptable vs Others

### 0.1 Adaptable Packages (Directly Manageable Through vcpkg)

| Package                        | Current State               | Platform Notes      | Adaptation Level                                              |
| ------------------------------ | --------------------------- | ------------------- | ------------------------------------------------------------- |
| boost-container                | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| boost-graph                    | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| sfmt                           | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| rapidjson                      | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| taskflow                       | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| sdl2                           | Already in `vcpkg.json`     | windows, osx, linux | Done (maintain)                                               |
| vulkan-memory-allocator        | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| imgui (+ docking-experimental) | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| gtest                          | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| glslang                        | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| spirv-cross                    | Already in `vcpkg.json`     | all                 | Done (maintain)                                               |
| directx-dxc                    | Already in `vcpkg.json`     | windows             | Done (maintain)                                               |
| assimp                         | vcpkg feature `editor`      | windows, osx        | Done (maintain)                                               |
| meshoptimizer                  | vcpkg feature `editor`      | windows, osx        | Done (maintain)                                               |
| stb                            | vcpkg feature `editor`      | windows, osx        | Done (maintain)                                               |
| imguizmo                       | vcpkg feature `editor`      | windows, osx        | Done (maintain)                                               |
| gklib                          | vcpkg feature `editor`      | windows, osx        | Adaptable (custom registry hardening needed)                  |
| metis                          | vcpkg feature `editor`      | windows, osx        | Done (maintain)                                               |
| ispc-texcomp                   | vcpkg feature `editor`      | windows, osx        | Adaptable (custom registry hardening needed)                  |
| bullet3                        | vcpkg feature `bullet`      | all                 | Done (maintain)                                               |
| recastnavigation               | vcpkg feature `recast`      | all                 | Done (maintain)                                               |
| tracy                          | vcpkg feature `tracy`       | all                 | Done (maintain)                                               |
| freetype                       | vcpkg feature `freetype`    | all                 | Done (maintain)                                               |
| lz4                            | vcpkg feature `compression` | all                 | Done (maintain)                                               |
| zlib                           | transitive today            | all                 | Adaptable (optional explicit pin if reproducibility required) |

### 0.2 Other Packages (Not Yet In vcpkg Flow, Or Should Stay Outside)

| Package / Dependency                 | Why It Is Not In Current vcpkg Flow                                                                                     | Recommendation                                                                           |
| ------------------------------------ | ----------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
| OpenXR (openxr-loader)               | XR plugin uses legacy `FindOpenXR.cmake`, no vcpkg alias in `cmake/vcpkg.cmake`                                         | Adapt to vcpkg in phase 2                                                                |
| Python runtime/dev (Python3::Python) | Resolved from host environment; version can drift across machines                                                       | Decide policy: keep system Python or pin with vcpkg/python3                              |
| Qt5 Widgets                          | Treated as host SDK/framework dependency for editor/tooling                                                             | Keep external for now; evaluate Qt via vcpkg only if full toolchain migration is desired |
| Vulkan SDK (Vulkan::Vulkan)          | Usually provided by OS SDK / LunarG SDK, not locked by vcpkg manifest                                                   | Keep external SDK dependency                                                             |
| game-activity (Android)              | Android NDK ecosystem package, outside vcpkg mainstream flow                                                            | Keep NDK-managed                                                                         |
| GLES/EGL libs                        | Platform graphics runtime libs, often system-provided                                                                   | Keep system/SDK-managed                                                                  |
| ios-cmake                            | Build toolchain helper rather than runtime package                                                                      | Keep as tool dependency                                                                  |
| crc32c in attached custom registry   | Registry contains crc32c port but project currently references `crc32` package name and does not consume it in manifest | Reconcile naming and consume only if external crc32c is needed                           |

## 1) Adaptation Tasks For Each Adaptable Package

Task format:
- T1: Align version strategy (overrides / baseline)
- T2: Verify target mapping in `cmake/vcpkg.cmake`
- T3: Validate platform guards and feature guards
- T4: Add CI lock checks (configure + build matrix)

| Package                  | Concrete Adaptation Tasks                                                                                                           |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------------------------- |
| boost-container          | T1 pin retained; T2 verify `Boost::container`; T4 verify all desktop builds                                                         |
| boost-graph              | T1 pin retained; T2 verify `Boost::graph`; T4 verify all desktop builds                                                             |
| sfmt                     | T1 keep version stable; T2 verify `sfmt::sfmt`; T4 test core module                                                                 |
| rapidjson                | T1 keep stable; T2 verify `rapidjson` include target alias; T4 run serialization tests                                              |
| taskflow                 | T1 keep stable; T2 verify `Taskflow::Taskflow`; T4 run task scheduler tests                                                         |
| sdl2                     | T1 keep override; T2 verify `SDL2::SDL2` + `SDL2::SDL2main`; T3 validate non-mobile guards                                          |
| vulkan-memory-allocator  | T1 keep override; T2 verify `GPUOpen::VulkanMemoryAllocator`; T4 run render backend tests                                           |
| imgui                    | T1 keep feature `docking-experimental`; T2 verify alias; T4 run editor UI build                                                     |
| gtest                    | T1 keep stable; T2 verify `GTest::gtest`; T4 execute unit tests in CI                                                               |
| glslang                  | T1 keep override; T2 verify component aliases; T4 run shader compile tests                                                          |
| spirv-cross              | T1 keep override; T2 verify six component package lookups; T4 run shader reflection tests                                           |
| directx-dxc              | T1 windows-only lock; T2 verify `Microsoft::DirectXShaderCompiler`; T3 enforce WIN32 guard                                          |
| assimp                   | T1 feature-scoped version lock; T2 verify `assimp::assimp`; T3 editor-only guard                                                    |
| meshoptimizer            | T1 feature-scoped lock; T2 verify `meshoptimizer::meshoptimizer`; T3 editor-only guard                                              |
| stb                      | T1 ensure registry/default source is deterministic; T2 verify include alias creation; T3 editor-only guard                          |
| imguizmo                 | T1 feature-scoped lock; T2 verify `imguizmo::imguizmo`; T3 editor-only guard                                                        |
| gklib                    | T1 add/verify custom port baseline and version DB; T2 verify `gklib::gklib`; T3 editor-only + os support checks                     |
| metis                    | T1 feature-scoped lock; T2 verify `metis::metis`; T3 editor-only + os support checks                                                |
| ispc-texcomp             | T1 add/verify custom port baseline and version DB; T2 verify `ispc_texcomp::ispc_texcomp`; T3 editor-only + os support checks       |
| bullet3                  | T1 feature-scoped lock; T2 verify all Bullet component targets; T4 run physics plugin tests                                         |
| recastnavigation         | T1 feature-scoped lock; T2 verify RecastNavigation target set; T4 run navmesh plugin tests                                          |
| tracy                    | T1 keep override; T2 verify `Tracy::TracyClient`; T3 ensure compile define `TRACY_ENABLE` only when enabled                         |
| freetype                 | T1 feature-scoped lock; T2 verify `Freetype::Freetype`; T4 run text rendering tests                                                 |
| lz4                      | T1 feature-scoped lock; T2 verify `lz4::lz4`; T4 run compression plugin tests                                                       |
| zlib (optional explicit) | T1 add explicit dependency only if direct use required; T2 wire alias if promoted to direct package; T4 verify no duplicate linkage |

## 2) Other Packages: Reasons And Adaptation Plan

### OpenXR
Reason:
- XR code depends on `3rdParty::OpenXR`, but vcpkg bridge does not yet map this target.

Adaptation plan:
1. Add `openxr-loader` dependency under a new vcpkg feature (for example `xr`).
2. Add `find_package(OpenXR CONFIG REQUIRED)` + alias `3rdParty::OpenXR` in `cmake/vcpkg.cmake`.
3. Keep platform guard matching `SKY_BUILD_XR` behavior.
4. Validate on at least Windows + one Unix-like target.

### Python3
Reason:
- Current vcpkg mode uses `find_package(Python3 COMPONENTS Development REQUIRED)` from host environment.
- This is less reproducible than manifest-pinned dependency resolution.

Adaptation plan:
1. Decide policy: reproducibility-first or host-Python-first.
2. If reproducibility-first, add `python3` to dedicated feature and use imported targets from vcpkg prefix.
3. Keep dynamic runtime loading strategy in plugin unchanged.
4. Add CI check that embeds Python and loads plugin successfully.

### Qt5
Reason:
- Qt tooling (moc/uic/rcc + platform plugins) is often managed as host SDK.
- Migrating to vcpkg is feasible but high-touch and developer-environment sensitive.

Adaptation plan:
1. Short term: keep external SDK and document required version.
2. Mid term: prototype vcpkg-based Qt on one platform in a branch.
3. Long term: either fully migrate or formalize external-SDK standard with bootstrap checks.

### Vulkan SDK
Reason:
- Vulkan loader/headers and validation ecosystem are usually handled by system SDK.

Adaptation plan:
1. Keep as external requirement.
2. Add preconfigure diagnostics for missing Vulkan SDK to improve onboarding.

### game-activity / GLES / ios-cmake
Reason:
- These are platform-ecosystem dependencies (Android/iOS/system graphics stack), not ideal as normal cross-platform vcpkg manifest items.

Adaptation plan:
1. Keep external toolchain/SDK ownership.
2. Add platform-specific setup docs and configure-time checks.
3. Add CI smoke jobs on relevant platform targets.

### crc32c Custom Registry Entry (Attached Registry)
Reason:
- Attached custom registry includes `crc32c`, while project config registers package name `crc32`.
- Current engine CRC32 implementation is internal and does not consume this package from `vcpkg.json`.

Adaptation plan:
1. Decide whether external crc32/c library is needed at all.
2. If needed, normalize naming (`crc32c` preferred for clarity) across:
   - custom registry package name
   - `vcpkg-configuration.json` `packages` list
   - `vcpkg.json` dependencies
3. If not needed, remove stale registry entry from project config to reduce confusion.

## 4) Execution Plan And Adaptation Roadmap

### Phase A (Stabilize Existing vcpkg Flow, 1-2 days)
1. Freeze dependency policy for current manifest features.
2. Validate all existing vcpkg aliases and feature toggles.
3. Add CI matrix: core, editor, bullet, recast, freetype, compression, tracy.
4. Deliverable: stable reproducible builds across supported desktop targets.

### Phase B (Custom Registry Governance, 1 day)
1. Decide single source of truth for custom ports:
   - external git registry only, or
   - in-repo registry only.
2. Reconcile custom package naming mismatch (`crc32` vs `crc32c`).
3. Add checklist for port add/update process (format, x-add-version, baseline bump).
4. Deliverable: deterministic custom registry consumption policy.

### Phase C (Expand vcpkg Coverage, 2-4 days)
1. Add `xr` feature and OpenXR package mapping.
2. Decide and implement Python strategy (host vs pinned vcpkg).
3. Add optional explicit zlib pin only if direct linkage is required.
4. Deliverable: wider vcpkg ownership without breaking platform SDK assumptions.

### Phase D (Platform And Tooling Hardening, ongoing)
1. Add configure-time diagnostics for Qt, Vulkan SDK, Android NDK, iOS toolchain.
2. Add developer bootstrap script for all non-vcpkg dependencies.
3. Audit quarterly dependency updates and lockfile/baseline changes.
4. Deliverable: lower onboarding cost and fewer environment-related failures.

## Recommended Immediate Actions

1. Fix the custom registry naming mismatch before further migration work.
2. Add XR dependency migration (OpenXR) as the next concrete package adaptation.
3. Decide Python dependency policy and codify it in vcpkg features.
4. Keep Qt/Vulkan/system graphics dependencies external until a dedicated migration spike is approved.
