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
| Rendering | `engine/render` | RHI backends, shader compiler, render core, adaptors, ImGui rendering |
| Animation | `engine/animation` | Animation runtime and related systems |
| Physics abstraction | `engine/physics` | Physics-facing engine interfaces |
| Navigation abstraction | `engine/navigation` | Navigation-facing engine interfaces |
| Editor | `engine/editor` | Qt-based editor runtime and tooling |
| Launcher | `engine/launcher` | Game/runtime executable entry point |
| Plugins | `plugins` | Optional modules such as Bullet, Recast, FreeType, Terrain, Console, Python, XR |
| Assets | `assets` | Shaders, materials, textures, splash data, techniques |
| Python tooling | `python` | Third-party bootstrap and project helper scripts |

## Repository Layout

```text
SkyEngine/
├── assets/          # Shaders, materials, textures, techniques
├── build_3rd/       # Generated third-party output and cache
├── cmake/           # Shared CMake configuration and helpers
├── configs/         # Runtime module manifests and presets
├── engine/          # Core engine modules, renderer, editor, launcher, tests
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

### Editor-specific dependencies

The editor is only built when `SKY_BUILD_EDITOR=ON` and Qt5 Widgets is available.

## Build Workflow

SkyEngine uses a two-stage build flow:

1. Bootstrap third-party packages with `python/third_party.py`
2. Configure and build the engine with CMake

If the third-party stage is skipped, CMake will not be able to resolve required dependencies.

### 1. Build third-party dependencies

Canonical command:

```bash
python3 python/third_party.py -p <platform> -j <jobs>
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
| `-i`, `--intermediate` | Download/build workspace, defaults to `build_3rd/intermediate` |
| `-o`, `--output` | Third-party output root, defaults to `build_3rd` |
| `-e`, `--engine` | Engine root, defaults to the current repository root |
| `-p`, `--platform` | Target platform |
| `-j`, `--jobs` | Parallel build jobs, `0` means auto |
| `-t`, `--target` | Build a single package |
| `-l`, `--list` | List known packages |
| `-f`, `--force` | Force rebuild even when cached metadata says up to date |
| `-c`, `--clean` | Clean package work trees |

Default output layout:

```text
build_3rd/
├── thirdparty_cache.cmake
├── Win32/
├── MacOS-x86/
├── MacOS-arm/
├── IOS/
├── Android/
└── Linux/
```

When the default layout is used, the effective `3RD_PATH` becomes the platform-specific subdirectory such as `build_3rd/MacOS-arm` or `build_3rd/Win32`. After a successful bootstrap, `build_3rd/thirdparty_cache.cmake` is regenerated, and the top-level CMake configure step can auto-load it when `3RD_PATH` is not set manually.

Examples:

```bash
# Build all third-party packages for Apple Silicon macOS
python3 python/third_party.py -p MacOS-arm -j 8

# Build all third-party packages for Windows
python3 python/third_party.py -p Win32 -j 8

# Build one package into a custom output root
python3 python/third_party.py -p Linux -o <thirdparty_root> -t taskflow
```

### 2. Configure CMake

General configure shape:

```bash
cmake -S . -B <build_dir> \
  -G <generator> \
  -D3RD_PATH=<thirdparty_platform_dir>
```

If `build_3rd/thirdparty_cache.cmake` exists, you can also omit `-D3RD_PATH=...` and let the repository load the cached value automatically.

Common CMake switches:

| Option | Description |
| --- | --- |
| `-DSKY_BUILD_EDITOR=ON` | Build the Qt editor |
| `-DSKY_BUILD_TEST=ON` | Enable test targets and `ctest` |
| `-DSKY_BUILD_TOOL=ON` | Enable render/tool-related build targets |
| `-DSKY_BUILD_BULLET=ON/OFF` | Toggle Bullet plugin |
| `-DSKY_BUILD_RECAST=ON/OFF` | Toggle Recast plugin |
| `-DSKY_BUILD_PYTHON=ON/OFF` | Toggle Python plugin |
| `-DSKY_BUILD_XR=ON/OFF` | Toggle XR plugin |
| `-DSKY_BUILD_FREETYPE=ON/OFF` | Toggle FreeType plugin |
| `-DSKY_BUILD_TERRAIN=ON/OFF` | Toggle Terrain plugin |
| `-DSKY_BUILD_PVS=ON/OFF` | Toggle PVS plugin |
| `-DSKY_BUILD_GUIZMO=ON/OFF` | Toggle ImGuizmo plugin, only meaningful with editor mode |
| `-DSKY_USE_TRACY=ON` | Enable Tracy profiling hooks |
| `-DSKY_MATH_SIMD=ON` | Enable SIMD math optimizations |

Platform examples:

```bash
# macOS Apple Silicon
cmake -S . -B build-macos-arm \
  -G Xcode \
  -D3RD_PATH=$PWD/build_3rd/MacOS-arm \
  -DSKY_BUILD_EDITOR=ON
```

```cmd
:: Windows
cmake -S . -B build-win32 ^
  -G "Visual Studio 17 2022" ^
  -D3RD_PATH=%cd%\build_3rd\Win32 ^
  -DSKY_BUILD_EDITOR=ON
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

Current repository defaults include:

- `SKY_BUILD_GUIZMO=ON` with editor-only gating
- `SKY_BUILD_PYTHON=OFF`
- `SKY_BUILD_XR=OFF`
- `SKY_BUILD_BULLET=ON`
- `SKY_BUILD_RECAST=ON`
- `SKY_BUILD_FREETYPE=ON`
- `SKY_BUILD_TERRAIN=ON`
- `SKY_BUILD_COMPRESSION=ON`
- `SKY_BUILD_PVS=ON`
- `SKY_BUILD_CONSOLE=ON`

This compile-time selection is separate from runtime module loading.

- `configs/modules_game.json` controls modules loaded by the game/runtime launcher
- `configs/modules_editor.json` controls modules loaded by the editor configuration

Keep both in sync when introducing a newly enabled plugin into a real runtime workflow.

## Main Targets

The top-level build commonly produces the following targets:

| Target | Purpose |
| --- | --- |
| `Launcher` | Main runtime executable |
| `Editor` | Qt-based editor executable when editor mode is enabled |
| `ShaderCompiler` / `ShaderCompiler.Static` | Shader compilation libraries |
| `ShaderTool` | Command-line shader tool target in the render shader module |
| `SkyRender` | Main render module |
| `SkyRender.Editor` | Editor render module when editor mode is enabled |

Some extra tools exist in the repository but are not all wired into the default top-level build path yet, so prefer the targets above unless you are extending the build graph yourself.

## Running

### Launcher

After a successful build, run the launcher from the generated output directory.

```bash
./output/bin/Launcher
```

On Windows, the launcher accepts `--app xr` to start XR application mode:

```cmd
output\bin\Launcher.exe --app xr
```

Depending on the generator, the actual executable may also live under a configuration subdirectory such as `output/bin/Release`.

### Editor

When built with `SKY_BUILD_EDITOR=ON` and Qt5 available:

```bash
./output/bin/Editor
```

## Assets And Shaders

The repository ships engine-side assets under `assets/`, including:

- HLSL shaders in `assets/shaders`
- material definitions in `assets/materials`
- render techniques in `assets/techniques`
- textures, fonts, and splash resources

Shader compilation support is provided by the `ShaderCompiler` libraries and `ShaderTool` target. The old README command that referenced `assets/shaders/compileshaders.py` is no longer a valid documented workflow in this repository.

## Notes

- `SKY_BUILD_EDITOR=ON` automatically enables `SKY_EDITOR=ON`
- Android uses a separate launcher project under `engine/launcher/android`
- The top-level `tools/` directory contains additional experiments and tooling targets, but the default build currently focuses on engine, plugins, and render-related tool targets

![SkyEngine screenshot](https://user-images.githubusercontent.com/35895395/195400282-ca50e99a-090b-4c52-a84c-d7e31a489e2f.png)
