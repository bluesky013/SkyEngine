# SkyEngine

SkyEngine is a modular C++20 game engine with a layered runtime, renderer, editor, plugin system, and Python-based build helpers. The repository targets desktop and mobile platforms and uses CMake as the primary build system.

## Overview

- Language and standard: C++20
- Build system: CMake
- Primary output location: `output/bin` for non-Android builds
- Supported third-party bootstrap platforms: `Win32`, `MacOS-x86`, `MacOS-arm`, `IOS`, `Android`, `Linux`

The codebase is organized around a small set of core engine modules and optional plugins:

| Area | Path | Purpose |
| --- | --- | --- |
| Core runtime | `engine/core` | Math, memory, file system, logging, async utilities |
| Application framework | `engine/framework` | Windowing, input, assets, world/application services |
| Rendering | `engine/aurora` | Aurora RHI, render graph (RDG), shader compiler, scene/ECS |
| Animation | `engine/animation` | Animation runtime and related systems |
| Physics abstraction | `engine/physics` | Physics-facing engine interfaces |
| Navigation abstraction | `engine/navigation` | Navigation-facing engine interfaces |
| Editor | `engine/sandbox` | Sandbox editor (engine-drawn UI) and its shell/module |
| Launcher | `<Project>/native` | Game/runtime executable entry point; the engine no longer ships a launcher |
| Plugins | `plugins` | Optional modules such as Bullet, Recast, Terrain, Vegetation, Audio, Console, Python |
| Assets | `assets` | Shaders, materials, textures, splash data, techniques |
| Python tooling | `python` | Third-party bootstrap and project helper scripts |

## Repository Layout

```text
SkyEngine/
├── assets/          # Shaders, materials, textures, techniques
├── cmake/           # Shared CMake configuration and helpers
├── configs/         # Runtime module manifests and presets
├── engine/          # Core engine modules, aurora renderer, sandbox editor, tests
├── openspec/        # Change proposals, specs, archived design work
├── plugins/         # Optional engine plugins controlled by CMake switches
├── python/          # Third-party bootstrap and Python utilities
└── tools/           # Extra tooling targets and experiments
```

## Prerequisites

### Required

- CMake 3.19 or newer
- Python 3
- Git
- A platform toolchain supported by CMake

### Python dependencies

Install the repository Python dependencies before using the helper scripts:

```bash
python3 -m pip install -r python/requirements.txt
```

### Editor build

The editor is the **sandbox editor** (`SandboxEditor` + `SandboxModule`), built with `-DSKY_BUILD_SANDBOX=ON`. It
uses the engine's own UI stack, not Qt.

## Build Workflow

SkyEngine uses a two-stage build flow:

1. Bootstrap third-party packages with `python/third_party.py`
2. Configure and build the engine with CMake

If the third-party stage is skipped, CMake will not be able to resolve required dependencies.

### 1. Build third-party dependencies

Canonical command:

```bash
python3 python/third_party.py -p <platform> -o <thirdparty_root> -j <jobs>
```

Supported platform values:

| Platform | Notes |
| --- | --- |
| `Win32` | Windows x86_64 |
| `MacOS-x86` | macOS Intel |
| `MacOS-arm` | macOS Apple Silicon |
| `IOS` | iOS arm64 |
| `Android` | Android arm64-v8a |
| `Linux` | Linux desktop |

Useful options:

| Option | Description |
| --- | --- |
| `-i`, `--intermediate` | Download/build workspace, defaults to `<output>/intermediate` |
| `-o`, `--output` | Required third-party output root, preferably outside the engine checkout |
| `-e`, `--engine` | Engine root, defaults to the current repository root |
| `-p`, `--platform` | Target platform |
| `-j`, `--jobs` | Parallel build jobs, `0` means auto |
| `-t`, `--target` | Build a single package |
| `-l`, `--list` | List known packages |
| `-f`, `--force` | Force rebuild even when cached metadata says up to date |
| `-c`, `--clean` | Clean package work trees |

Shared output layout (only built platforms are present):

```text
sky_3rd/
├── intermediate/                    # Source checkouts and regenerable build trees
├── thirdparty_cache_Win32.cmake     # Generated cache for each built platform
├── Win32/
├── MacOS-x86/
├── MacOS-arm/
├── IOS/
├── Android/
└── Linux/
```

`3RD_PATH` points to a platform subdirectory, such as `D:/Code/Engine/sky_3rd/Win32`; `-o` points to its parent. Multiple engine checkouts can consume the same installed packages when their dependency versions, build options and toolchains are compatible. Use a separate output root for incompatible versions, and do not run concurrent third-party builds into the same root. Metadata checks compare package declarations; they do not verify toolchain compatibility or file integrity.

Examples:

```bash
# Build all third-party packages for Apple Silicon macOS
python3 python/third_party.py -p MacOS-arm -o ../sky_3rd -j 8

# Build all third-party packages for Windows
python3 python/third_party.py -p Win32 -o D:/Code/Engine/sky_3rd -j 8

# Build one package into a custom output root
python3 python/third_party.py -p Linux -o <thirdparty_root> -t sfmt

# Check a shared Windows installation against this checkout's dependency list
python3 python/third_party.py -p Win32 -o D:/Code/Engine/sky_3rd --check --strict
```

### 2. Configure CMake

General configure shape:

```bash
cmake -S . -B <build_dir> \
  -G <generator> \
  -D3RD_PATH=<thirdparty_platform_dir>
```

For an external shared root, pass `-D3RD_PATH=D:/Code/Engine/sky_3rd/Win32` explicitly or put that cache variable in an ignored `CMakeUserPresets.json` preset. Existing build trees remember it. Legacy in-repository `build_3rd/thirdparty_cache_<platform>.cmake` (or `thirdparty_cache.cmake`) is still auto-loaded when `3RD_PATH` is empty.

Common CMake switches:

| Option | Description |
| --- | --- |
| `-DSKY_BUILD_SANDBOX=ON` | Build the sandbox editor and its shell/module |
| `-DSKY_BUILD_TEST=ON` | Enable test targets and `ctest` |
| `-DSKY_BUILD_TOOL=ON` | Build `tools/` developer targets |
| `-DSKY_BUILD_BULLET=ON/OFF` | Toggle Bullet plugin |
| `-DSKY_BUILD_RECAST=ON/OFF` | Toggle Recast plugin |
| `-DSKY_BUILD_TERRAIN=ON/OFF` | Toggle Terrain plugin |
| `-DSKY_BUILD_VEGETATION=ON/OFF` | Toggle Vegetation plugin |
| `-DSKY_BUILD_COMPRESSION=ON/OFF` | Toggle Compression plugin |
| `-DSKY_BUILD_PVS=ON/OFF` | Toggle PVS plugin |
| `-DSKY_BUILD_AUDIO=ON/OFF` | Toggle Audio plugin |
| `-DSKY_BUILD_CONSOLE=ON/OFF` | Toggle Console plugin |
| `-DSKY_BUILD_NETWORK_ENET=ON/OFF` | Toggle ENet network plugin |
| `-DSKY_BUILD_PYTHON=ON/OFF` | Toggle Python plugin |
| `-DSKY_BUILD_FREETYPE=ON/OFF` | Toggle the FreeType UI font provider |
| `-DSKY_USE_TRACY=ON` | Enable Tracy profiling hooks |
| `-DSKY_MATH_SIMD=ON` | Enable SIMD math optimizations |

Platform examples:

```bash
# macOS Apple Silicon
cmake -S . -B build-macos-arm \
  -G Xcode \
  -D3RD_PATH=$PWD/../sky_3rd/MacOS-arm \
  -DSKY_BUILD_SANDBOX=ON
```

```cmd
:: Windows
cmake -S . -B build-win32 ^
  -G "Visual Studio 17 2022" ^
  -D3RD_PATH=D:/Code/Engine/sky_3rd/Win32 ^
  -DSKY_BUILD_SANDBOX=ON
```

### 3. Build targets

Build the generated project:

```bash
cmake --build <build_dir> --config Release --parallel
```

For non-Android builds, executables and shared libraries are emitted under `output/bin` or `output/bin/<Config>` depending on the generator.

### 4. Run tests

Tests are only available when configured with `-DSKY_BUILD_TEST=ON`.

```bash
ctest --test-dir <build_dir> -C Release --output-on-failure
```

## Plugin Configuration

Compile-time plugin selection is driven by `plugins/plugins.json`. Each entry defines the plugin directory, the controlling CMake variable, and whether the plugin requires editor mode.

Current repository defaults (see `plugins/plugins.json`):

- `SKY_BUILD_BULLET=ON`
- `SKY_BUILD_RECAST=ON`
- `SKY_BUILD_TERRAIN=ON`
- `SKY_BUILD_VEGETATION=ON`
- `SKY_BUILD_COMPRESSION=ON`
- `SKY_BUILD_PVS=ON`
- `SKY_BUILD_AUDIO=ON`
- `SKY_BUILD_CONSOLE=ON`
- `SKY_BUILD_NETWORK_ENET=ON`
- `SKY_BUILD_PYTHON=OFF`

This compile-time selection is separate from runtime module loading.

- `configs/modules_game.json` controls modules loaded by the game/runtime launcher
- `configs/modules_editor.json` controls modules loaded by the editor configuration

Keep both in sync when introducing a newly enabled plugin into a real runtime workflow.

## Main Targets

The top-level build commonly produces the following targets:

| Target | Purpose |
| --- | --- |
| `SandboxEditor` | Sandbox editor executable (engine-drawn UI, non-Qt) |
| `SandboxModule` | Editor shell/module loaded at runtime by `SandboxEditor`; rebuild it when changing editor UI code |
| `Aurora.Render` | Aurora render module |
| `Aurora.DX12` / `Aurora.Vulkan` | Aurora RHI backends |
| `Aurora.ShaderCompiler` | Aurora shader compiler |
| `AssetTool` | Command-line asset cooking / packaging tool |

The runtime host is project-owned (generated under `<Project>/native`); the engine no longer ships a runtime launcher executable. Some extra tools exist in the repository but are not all wired into the default top-level build path yet, so prefer the targets above unless you are extending the build graph yourself.

## Running

### Running

The engine no longer ships its own runtime launcher; the runtime host is **project-owned**. Create a project and
build its launcher (see `docs/features/game-project-build.md`):

```bash
python3 python/package_project.py create --dir <parent> --name MyGame
python3 python/package_project.py generate --project <parent>/MyGame --preset MyGame-macos
cmake --build --preset macos   # in <Project>/native -> <Project>/build/<platform>/bin/<Name>
```

The editor is a separate host built as `SandboxEditor`.

### Editor

When built with `SKY_BUILD_SANDBOX=ON`:

```bash
./output/bin/<Config>/SandboxEditor
```

## Assets And Shaders

The repository ships engine-side assets under `assets/`, including:

- HLSL shaders in `assets/shaders`
- material definitions in `assets/materials`
- render techniques in `assets/techniques`
- textures, fonts, and splash resources

Shader compilation is provided by the Aurora Slang-based shader compiler (`Aurora.ShaderCompiler.Static` and the `Aurora.ShaderCompiler` module). The old README command that referenced `assets/shaders/compileshaders.py` is no longer a valid documented workflow in this repository.

## Notes

- The editor is the sandbox editor (`-DSKY_BUILD_SANDBOX=ON`)
- The runtime launcher is project-owned (generated under `<Project>/native` + `<Project>/build/<platform>`); the engine ships launcher **templates** under `engine/templates/launcher/`
- The top-level `tools/` directory contains additional experiments and tooling targets, but the default build currently focuses on engine, plugins, and aurora render tooling

![SkyEngine screenshot](https://user-images.githubusercontent.com/35895395/195400282-ca50e99a-090b-4c52-a84c-d7e31a489e2f.png)
