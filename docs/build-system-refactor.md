---
title: "SkyEngine Build System Refactor"
description: "Design document for a unified, dual-mode build system supporting local vcpkg builds and CI-published prebuilds."
module: "build-system"
updated: "2026-03-29"
---

## 1. Goals

| # | Goal |
|---|------|
| 1 | **Dual-mode support** – developers can choose either a *local build* (vcpkg compiles dependencies from source) or a *prebuild* mode (pre-compiled artifacts published by CI are downloaded automatically). |
| 2 | **vcpkg-first** – local mode exclusively uses vcpkg for version pinning, triplet selection, and feature gating; the legacy `python/third_party.py` / `3RD_PATH` path is retired. |
| 3 | **Transparent vcpkg discovery** – check for a global vcpkg installation first; fall back to cloning and bootstrapping a local copy only when no global instance is found. |
| 4 | **Prebuilds via CI/CD** – artifacts are built per-platform per-triplet in CI, published to a release/artifact store, and downloaded on demand during the engine CMake configure step. |
| 5 | **Simplicity** – the build system should be expressed in ≤ 5 CMake files with clear, single-responsibility roles and no duplicated logic. |
| 6 | **Full triplet coverage** – every declared dependency must build cleanly for all six supported triplets; a CMake helper selects the correct triplet automatically based on host/target platform. |

---

## 2. Current State Summary

### 2.1 File Layout

```
CMakeLists.txt                  top-level: vcpkg feature mapping + project()
cmake/
  options.cmake                 SKY_BUILD_* option declarations
  configuration.cmake           platform compile-flag setup
  thirdparty.cmake              mode dispatch: vcpkg | 3RD_PATH | fatal error
  vcpkg.cmake                   vcpkg mode: find_package + 3rdParty:: alias targets
  vcpkg_bootstrap.cmake         clone/pin microsoft/vcpkg locally
  thirdparty_helpers.cmake      shared helpers (sky_vcpkg_alias, etc.)
  thirdparty/                   legacy Find*.cmake modules (3RD_PATH mode)
  thirdparty.json               package list for python/third_party.py
python/
  third_party.py                legacy builder: git-clone + cmake + archive
triplets/
  arm64-android.cmake
  arm64-ios.cmake
  arm64-osx.cmake
  x64-linux.cmake
  x64-osx.cmake
  x64-windows-static.cmake
vcpkg.json                      manifest with version pins and feature gates
vcpkg-configuration.json        registry sources (microsoft/vcpkg + custom)
```

### 2.2 Known Pain Points

| Pain Point | Detail |
|---|---|
| Two divergent paths | vcpkg mode and 3RD_PATH mode share no code and can silently diverge in what gets linked. |
| `vcpkg_bootstrap.cmake` always clones | Does not probe `VCPKG_ROOT` or `PATH` before initiating a network clone. |
| Triplet auto-detection is incomplete | `thirdparty.cmake` only maps `WIN32 → x64-windows` and `UNIX → x64-linux`; macOS ARM, iOS, Android are never set automatically. |
| No prebuild download path | Prebuild archives exist on disk (python script creates them) but there is no CMake-side logic to fetch them from a remote store. |
| Legacy mode can diverge | `python/third_party.py` clones specific git tags; package versions can drift from vcpkg-pinned versions. |
| `x64-windows` vs `x64-windows-static` mismatch | CI sets `VCPKG_DEFAULT_TRIPLET=x64-windows-static`, but `thirdparty.cmake` picks `x64-windows` on Windows (a different triplet). |

---

## 3. Proposed Architecture

### 3.1 Overview

```
CMakeLists.txt
└── cmake/sky_build_mode.cmake      ← NEW: single entry point, picks mode
    ├── [mode = local-vcpkg]
    │   ├── cmake/sky_vcpkg_find.cmake   ← NEW: global vcpkg probe / bootstrap
    │   ├── cmake/sky_triplet.cmake      ← NEW: triplet auto-selection
    │   └── cmake/sky_packages.cmake     ← RENAMED from vcpkg.cmake
    └── [mode = prebuild]
        └── cmake/sky_prebuild.cmake     ← NEW: download + unpack prebuilts
```

All downstream engine code continues to consume `3rdParty::<name>` interface targets regardless of mode; only the two leaf cmake files differ.

### 3.2 Mode Selection

A single CMake option controls the mode:

```cmake
# cmake/options.cmake  (new addition)
option(SKY_PREBUILD_MODE "Use pre-compiled third-party artifacts instead of building from source" OFF)
```

When `SKY_PREBUILD_MODE=ON`:

1. CMake reads the artifact metadata from `cmake/prebuild_manifest.json` (see §5).
2. Downloads the archive for the current triplet if not already present.
3. Unpacks to `${CMAKE_BINARY_DIR}/_prebuilts/<triplet>/`.
4. Sets `CMAKE_PREFIX_PATH` so that `find_package` resolves against the unpacked tree.
5. Includes `cmake/sky_packages.cmake` as usual — all `find_package` + alias calls are identical to local mode.

When `SKY_PREBUILD_MODE=OFF` (default):

1. Runs vcpkg discovery and optional bootstrap.
2. Sets `CMAKE_TOOLCHAIN_FILE` to the located vcpkg toolchain.
3. vcpkg manifest mode installs exactly the packages listed in `vcpkg.json`.

---

## 4. vcpkg Discovery and Bootstrap

### 4.1 Search Order (`cmake/sky_vcpkg_find.cmake`)

```
1. VCPKG_ROOT environment variable (user-managed global install)
2. CMAKE_TOOLCHAIN_FILE already pointing at vcpkg (passed on command line)
3. Well-known system locations:
     Windows : %LOCALAPPDATA%\vcpkg  |  C:\vcpkg
     macOS/Linux : $HOME/.vcpkg  |  /usr/local/vcpkg  |  /opt/vcpkg
4. Local clone at ${CMAKE_SOURCE_DIR}/vcpkg  (created by bootstrap if needed)
```

Only step 4 triggers a network operation.  The existing `cmake/vcpkg_bootstrap.cmake` is refactored into a private sub-function called exclusively from step 4.

```cmake
# cmake/sky_vcpkg_find.cmake  (pseudocode)

function(sky_find_or_bootstrap_vcpkg OUT_TOOLCHAIN)
    # Steps 1-3: probe candidates
    foreach(candidate IN LISTS _sky_vcpkg_candidates)
        if(EXISTS "${candidate}/scripts/buildsystems/vcpkg.cmake")
            set(${OUT_TOOLCHAIN} "${candidate}/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
            message(STATUS "[SkyEngine] Found vcpkg at ${candidate}")
            return()
        endif()
    endforeach()

    # Step 4: bootstrap local clone
    message(STATUS "[SkyEngine] No global vcpkg found – bootstrapping local copy")
    _sky_vcpkg_bootstrap()  # clone + pin + compile vcpkg binary
    set(${OUT_TOOLCHAIN} "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" PARENT_SCOPE)
endfunction()
```

The bootstrap function retains the git-pin-to-baseline logic from the existing `vcpkg_bootstrap.cmake`, but pins the commit from `vcpkg-configuration.json`'s `default-registry.baseline` automatically, removing the separate hardcoded SHA.

### 4.2 Usage in `CMakeLists.txt`

```cmake
# Must run before project()
if(NOT SKY_PREBUILD_MODE)
    include(cmake/sky_vcpkg_find.cmake)
    sky_find_or_bootstrap_vcpkg(_sky_toolchain)
    set(CMAKE_TOOLCHAIN_FILE "${_sky_toolchain}" CACHE STRING "" FORCE)
endif()

set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "" FORCE)
# ... feature mapping ...
project(SkyEngine)
```

---

## 5. Triplet Selection

### 5.1 Current Triplets

| Triplet file | OS | Arch | CRT | Library |
|---|---|---|---|---|
| `x64-windows-static` | Windows | x64 | static | static |
| `x64-linux` | Linux | x64 | dynamic | static |
| `x64-osx` | macOS | x86_64 | dynamic | static |
| `arm64-osx` | macOS | arm64 | dynamic | static |
| `arm64-ios` | iOS | arm64 | dynamic | static |
| `arm64-android` | Android | arm64 | dynamic | static |

### 5.2 Triplet Issues

| Issue | Detail |
|---|---|
| No `x64-windows` triplet | CI sets `x64-windows-static` but old thirdparty.cmake detected `x64-windows`. Should be unified to `x64-windows-static` on all paths. |
| `x64-linux` missing `VCPKG_BUILD_TYPE` | Should set `VCPKG_BUILD_TYPE release` for CI prebuilts (release-only) or leave blank for dev (debug+release). |
| `arm64-android` missing toolchain vars | Does not set `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` pointing to NDK toolchain, which causes failures for some packages. |
| No `x86_64-android` triplet | Some older Android devices still use x86_64; add if XR or broad device support is required. |

### 5.3 Auto-Selection Logic (`cmake/sky_triplet.cmake`)

```cmake
# cmake/sky_triplet.cmake

function(sky_select_triplet OUT_TRIPLET)
    # Cross-compile targets override host detection
    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        set(_triplet "arm64-android")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_triplet "arm64-ios")
    elseif(WIN32)
        set(_triplet "x64-windows-static")
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        # Apple Silicon vs Intel
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64" OR
           CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
            set(_triplet "arm64-osx")
        else()
            set(_triplet "x64-osx")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_triplet "x64-linux")
    else()
        message(FATAL_ERROR "[SkyEngine] Unsupported platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    set(${OUT_TRIPLET} "${_triplet}" PARENT_SCOPE)
    message(STATUS "[SkyEngine] Auto-selected triplet: ${_triplet}")
endfunction()
```

This replaces the ad-hoc `if(WIN32)` / `elseif(UNIX)` block in `thirdparty.cmake` and is called from `CMakeLists.txt` before `project()`.

### 5.4 Triplet Fixes Required

| Triplet | Required Change |
|---|---|
| `arm64-android.cmake` | Add `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` pointing to `$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake`; set `VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31`. |
| `x64-linux.cmake` | Add `set(VCPKG_BUILD_TYPE release)` when building release-only CI artifacts (controlled by a `VCPKG_RELEASE_ONLY` overlay or CI env var). |
| `arm64-ios.cmake` | Verify `VCPKG_OSX_DEPLOYMENT_TARGET 13.0` is honored by all port portfiles; add note about required Xcode version. |

---

## 6. Package Compatibility Matrix

The table below maps each package against each triplet and marks known blockers.

| Package | x64-win-static | x64-linux | x64-osx | arm64-osx | arm64-ios | arm64-android |
|---|---|---|---|---|---|---|
| boost-container | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| boost-graph | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| sfmt | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| rapidjson | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| taskflow | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| sdl2 | ✅ | ✅ | ✅ | ✅ | ❌ (no SDL on iOS) | ❌ (no SDL on Android) |
| vulkan-memory-allocator | ✅ | ✅ | ✅ | ✅ | ⚠️ MoltenVK only | ✅ |
| imgui | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| glslang | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| spirv-cross | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| directx-dxc | ✅ | ❌ (WIN32 only) | ❌ (WIN32 only) | ❌ | ❌ | ❌ |
| gtest | ✅ | ✅ | ✅ | ✅ | ⚠️ no runner | ⚠️ no runner |
| lz4 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| freetype | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| tracy | ✅ | ✅ | ✅ | ✅ | ⚠️ limited | ⚠️ limited |
| bullet3 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| recastnavigation | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| assimp (editor) | ✅ | ❌ (editor=win/osx) | ✅ | ✅ | ❌ | ❌ |
| meshoptimizer (editor) | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ |
| stb (editor) | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ |
| imguizmo (editor) | ✅ | ❌ | ✅ | ✅ | ❌ | ❌ |
| gklib (editor) | ✅ | ❌ | ✅ | ⚠️ custom port | ❌ | ❌ |
| metis (editor) | ✅ | ❌ | ✅ | ⚠️ custom port | ❌ | ❌ |
| ispc-texcomp (editor) | ✅ | ❌ | ✅ | ⚠️ ISPC compiler needed | ❌ | ❌ |

Legend: ✅ known-good | ⚠️ needs verification or minor fix | ❌ unsupported by design

**Rule for platform guards in `vcpkg.json`:**

```json
{ "name": "sdl2",     "platform": "windows | osx | linux" }
{ "name": "directx-dxc", "platform": "windows" }
```

Editor feature should restrict its dependencies to `"platform": "windows | osx"` as it does today; this is correct.

---

## 7. Prebuild Mode: CI/CD Pipeline and Download

### 7.1 Artifact Naming Convention

Each CI run that publishes prebuilts produces one archive per triplet:

```
skyengine-prebuilts-<triplet>-<vcpkg_baseline_short>.tar.zst
```

Example: `skyengine-prebuilts-x64-windows-static-c3867e71.tar.zst`

The archive is a flat install tree matching the vcpkg installed layout:

```
<triplet>/
  include/
  lib/
  share/       ← cmake config files live here
  bin/         ← DLLs / dylibs (when LIBRARY_LINKAGE=dynamic)
  debug/
    lib/
    bin/
```

### 7.2 Prebuild Manifest (`cmake/prebuild_manifest.json`)

```json
{
  "vcpkg_baseline": "c3867e714dd3a51c272826eea77267876517ed99",  // microsoft/vcpkg tag 2026.03.18
  "triplets": {
    "x64-windows-static": {
      "url": "https://github.com/bluesky013/SkyEngine/releases/download/prebuilts-20260329/skyengine-prebuilts-x64-windows-static-c3867e71.tar.zst",
      "sha256": "<hash>"
    },
    "x64-linux": {
      "url": "...",
      "sha256": "<hash>"
    },
    "x64-osx": { "url": "...", "sha256": "<hash>" },
    "arm64-osx": { "url": "...", "sha256": "<hash>" },
    "arm64-ios": { "url": "...", "sha256": "<hash>" },
    "arm64-android": { "url": "...", "sha256": "<hash>" }
  }
}
```

This file is committed to the repository and updated by an automated CI step when a new prebuild batch is published.

### 7.3 CMake Download Logic (`cmake/sky_prebuild.cmake`)

```cmake
# cmake/sky_prebuild.cmake  (pseudocode)

function(sky_download_prebuilts TRIPLET)
    file(READ "${CMAKE_SOURCE_DIR}/cmake/prebuild_manifest.json" _manifest)
    string(JSON _url    GET "${_manifest}" "triplets" "${TRIPLET}" "url")
    string(JSON _sha256 GET "${_manifest}" "triplets" "${TRIPLET}" "sha256")

    set(_archive_dir "${CMAKE_BINARY_DIR}/_prebuilts")
    set(_archive     "${_archive_dir}/${TRIPLET}.tar.zst")
    set(_unpack_dir  "${_archive_dir}/${TRIPLET}")

    # Skip if already unpacked and manifest hash matches
    set(_stamp "${_unpack_dir}/.stamp_${_sha256}")
    if(EXISTS "${_stamp}")
        message(STATUS "[SkyEngine] Prebuilts for ${TRIPLET} are up-to-date")
    else()
        message(STATUS "[SkyEngine] Downloading prebuilts for ${TRIPLET} ...")
        file(DOWNLOAD "${_url}" "${_archive}"
            EXPECTED_HASH SHA256=${_sha256}
            SHOW_PROGRESS
            STATUS _dl_status
        )
        list(GET _dl_status 0 _dl_code)
        if(NOT _dl_code EQUAL 0)
            message(FATAL_ERROR "[SkyEngine] Failed to download prebuilts: ${_dl_status}")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_archive}" DESTINATION "${_unpack_dir}")
        file(TOUCH "${_stamp}")
        message(STATUS "[SkyEngine] Prebuilts extracted to ${_unpack_dir}")
    endif()

    # Expose to find_package
    list(PREPEND CMAKE_PREFIX_PATH "${_unpack_dir}")
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
endfunction()
```

After `sky_download_prebuilts` runs, `cmake/sky_packages.cmake` (the renamed `vcpkg.cmake`) is included and all `find_package` calls resolve against the unpacked tree without any code changes.

### 7.4 CI Pipeline Design

```
┌─────────────────────────────────────────────────────────────┐
│  GitHub Actions: "Publish Prebuilts" (manual or tag-driven) │
│                                                             │
│  Matrix:                                                    │
│    triplet: [x64-windows-static, x64-linux, x64-osx,       │
│              arm64-osx, arm64-ios, arm64-android]           │
│                                                             │
│  Steps per triplet:                                         │
│  1. Checkout                                                │
│  2. Setup vcpkg (lukka/run-vcpkg@v11 with baseline commit)  │
│  3. cmake configure (vcpkg toolchain, selected triplet,     │
│       SKY_BUILD_EDITOR=ON, all optional features ON)        │
│  4. cmake --build (no engine code, just vcpkg install)      │
│     Tip: use "cmake -P cmake/vcpkg_export.cmake" to export  │
│     only the installed tree without building engine code.   │
│  5. Pack: tar --zstd -cf <triplet>.tar.zst installed/       │
│  6. Upload artifact to GitHub Release                       │
│                                                             │
│  Final step (after matrix):                                 │
│  7. Update cmake/prebuild_manifest.json with new URLs+SHA   │
│  8. Open automated PR to commit the updated manifest        │
└─────────────────────────────────────────────────────────────┘
```

The CI job that builds the engine (push/PR workflow) is separate and uses `SKY_PREBUILD_MODE=ON` by default to avoid recompiling dependencies on every build.

---

## 8. Unified CMake Entry Point

After the refactor, `CMakeLists.txt` becomes:

```cmake
cmake_minimum_required(VERSION 3.19)

# ---- Options needed before project() ----
include(cmake/options.cmake)       # SKY_BUILD_*, SKY_PREBUILD_MODE
include(cmake/sky_triplet.cmake)   # sky_select_triplet()

sky_select_triplet(VCPKG_TARGET_TRIPLET)
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "" FORCE)

if(NOT SKY_PREBUILD_MODE)
    include(cmake/sky_vcpkg_find.cmake)
    sky_find_or_bootstrap_vcpkg(_sky_toolchain)
    set(CMAKE_TOOLCHAIN_FILE "${_sky_toolchain}" CACHE STRING "" FORCE)
    include(cmake/sky_vcpkg_features.cmake)  # map SKY_BUILD_* → VCPKG_MANIFEST_FEATURES
endif()

project(SkyEngine)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(ENGINE_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(cmake/functions.cmake)
include(cmake/plugin_switches.cmake)
include(cmake/configuration.cmake)

# ---- Third-party resolution ----
if(SKY_PREBUILD_MODE)
    include(cmake/sky_prebuild.cmake)
    sky_download_prebuilts("${VCPKG_TARGET_TRIPLET}")
endif()
include(cmake/sky_packages.cmake)   # renamed vcpkg.cmake; works in both modes

# ---- Engine targets ----
if(NOT ANDROID)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ENGINE_ROOT}/output/bin)
endif()

if(SKY_BUILD_TEST)
    enable_testing()
endif()

add_subdirectory(engine)
add_subdirectory(plugins)
if(SKY_BUILD_TOOL)
    add_subdirectory(tools)
endif()
```

---

## 9. File Responsibility Map (After Refactor)

| File | Responsibility |
|---|---|
| `CMakeLists.txt` | Top-level: option wiring, triplet selection, vcpkg/prebuild dispatch, `project()`, add_subdirectory |
| `cmake/options.cmake` | All `option()` and `set(CACHE ...)` declarations |
| `cmake/sky_triplet.cmake` | Auto-detect correct vcpkg triplet from CMake platform/arch variables |
| `cmake/sky_vcpkg_find.cmake` | Probe global vcpkg installations; bootstrap local clone as last resort |
| `cmake/sky_vcpkg_features.cmake` | Map `SKY_BUILD_*` options to `VCPKG_MANIFEST_FEATURES` (moved out of CMakeLists.txt) |
| `cmake/sky_packages.cmake` | `find_package` calls + `3rdParty::` alias targets; mode-agnostic |
| `cmake/sky_prebuild.cmake` | Read `prebuild_manifest.json`, download and unpack prebuilt archive |
| `cmake/prebuild_manifest.json` | Maps triplet → download URL + SHA256; updated by CI automation |
| `cmake/configuration.cmake` | Compile flags, platform defines (unchanged) |
| `cmake/functions.cmake` | sky_add_exe, sky_add_lib helpers (unchanged) |
| `triplets/*.cmake` | vcpkg triplet definitions (see §5.4 for required fixes) |
| `vcpkg.json` | Dependency manifest with version overrides and feature gates |
| `vcpkg-configuration.json` | Registry sources (unchanged) |

Files **removed** or retired after the refactor:

| File | Disposition |
|---|---|
| `cmake/thirdparty.cmake` | Replaced by `sky_packages.cmake` + `sky_prebuild.cmake` |
| `cmake/vcpkg.cmake` | Renamed/merged into `sky_packages.cmake` |
| `cmake/vcpkg_bootstrap.cmake` | Functionality absorbed into `sky_vcpkg_find.cmake` |
| `cmake/thirdparty_helpers.cmake` | Merged into `sky_packages.cmake` (one small function) |
| `python/third_party.py` | Retired; prebuild artifacts are now produced by CI vcpkg |
| `cmake/thirdparty.json` | Retired; packages are tracked in `vcpkg.json` |
| `cmake/thirdparty/Find*.cmake` | Retired; only `find_package` with vcpkg prefix paths needed |

---

## 10. Migration Checklist

### Phase 1: vcpkg Stabilization (1–2 days)

- [ ] Fix triplet auto-selection (`sky_triplet.cmake`) and remove hardcoded `x64-windows` / `x64-linux` strings from thirdparty.cmake
- [ ] Unify Windows triplet to `x64-windows-static` in all paths (CMakeLists.txt, CI yaml)
- [ ] Fix `arm64-android.cmake`: add `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` and configure options
- [ ] Verify global vcpkg probe order; refactor `vcpkg_bootstrap.cmake` to check env vars first
- [ ] Validate all `find_package` + alias pairs in `cmake/vcpkg.cmake` for all desktop triplets in CI
- [ ] Remove or guard deprecated `cxxopts` target (present in legacy list but absent from vcpkg.json)

### Phase 2: Prebuild Infrastructure (2–3 days)

- [ ] Define `cmake/prebuild_manifest.json` schema
- [ ] Write `cmake/sky_prebuild.cmake` (download, verify SHA256, unpack, set `CMAKE_PREFIX_PATH`)
- [ ] Add `SKY_PREBUILD_MODE` option to `cmake/options.cmake`
- [ ] Create CI workflow `publish-prebuilts.yml` with matrix across all triplets
- [ ] Add automated manifest-update step and PR creation to CI pipeline
- [ ] Validate that `sky_packages.cmake` resolves identically in both modes

### Phase 3: Cleanup and Documentation (1 day)

- [ ] Remove `python/third_party.py` and `cmake/thirdparty.json` (or archive in a `legacy/` folder)
- [ ] Remove `cmake/thirdparty/` Find modules
- [ ] Update top-level `README.md` with new build instructions (local-build and prebuild)
- [ ] Update `.github/workflows/cmake.yml`: replace legacy job with prebuild-mode job

---

## 11. Developer Usage After Refactor

### Local Build (vcpkg auto-bootstrapped)

```bash
# First time: vcpkg is found via VCPKG_ROOT or auto-bootstrapped
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Local Build (explicit global vcpkg)

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Prebuild Mode (fast, no source compilation of dependencies)

```bash
cmake -B build \
  -DSKY_PREBUILD_MODE=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The first configure run downloads and unpacks `~300 MB` of prebuilt libraries for the detected triplet.  Subsequent configures use the cached unpack directory (verified by SHA256 stamp).

### Cross-Compile for Android

```bash
cmake -B build \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-31 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Triplet `arm64-android` is selected automatically by `sky_triplet.cmake`.
