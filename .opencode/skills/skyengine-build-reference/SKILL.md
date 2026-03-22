---
name: skyengine-build-reference
description: Follow the SkyEngine cross-platform build workflow, including third-party bootstrap, platform-specific CMake generation, engine build, Android packaging, and optional test execution.
compatibility: opencode
metadata:
  source: README.md, python/third_party.py, repository inspection
  audience: contributors
---

# SkyEngine Cross-Platform Build Reference

Use this skill when the task involves building SkyEngine, proposing build commands, or explaining platform-specific build constraints.

## Working rules

- Always keep the documented order: third-party bootstrap first, then CMake configure/generate, then engine build, then optional tests or app packaging.
- Treat `python/third_party.py` as the required entrypoint for third-party setup and `3RD_PATH` generation.
- Preserve placeholders like `<engine_root>`, `<build_dir>`, and `<thirdparty_output_dir>` unless the user provides concrete paths.
- Use only platform names that exist in the repository tooling: `Win32`, `MacOS-x86`, `MacOS-arm`, `IOS`, `Android`, `Linux`.
- Call out platform assumptions explicitly when adapting commands.
- Verify whether `SKY_BUILD_TEST=ON` is enabled before recommending `ctest`.
- Mention required SDK/toolchain constraints when relevant, especially for Android and iOS.

## Canonical workflow

SkyEngine uses this order on every platform:

1. Build third-party dependencies with `python/third_party.py`
2. Configure CMake with `-D3RD_PATH=<thirdparty_output_dir>`
3. Build the engine targets
4. Optionally run tests or package the platform launcher

If `3RD_PATH` does not exist, CMake-side third-party discovery fails.

## Step 1: Build third-party dependencies (required)

Canonical command template:

```bash
python3 python/third_party.py \
  -i <thirdparty_intermediate_dir> \
  -o <thirdparty_output_dir> \
  -e <engine_root> \
  -p <platform> \
  -j <jobs>
```

Common options:

- `-i, --intermediate`: clone/build workspace for 3rd-party sources
- `-o, --output`: installed third-party output used as `3RD_PATH`
- `-e, --engine`: repository root
- `-p, --platform`: one of `Win32`, `MacOS-x86`, `MacOS-arm`, `IOS`, `Android`, `Linux`
- `-j, --jobs`: build parallelism (`0` means auto in the script)
- `-f, --force`: force rebuild even if cached metadata says up to date
- `-c, --clean`: clean package working trees
- `-t, --target`: build a single package
- `-l, --list`: list packages and supported platforms

Useful examples:

```bash
# List available packages
python3 python/third_party.py -e <engine_root> -p Win32 --list

# Force full rebuild for macOS arm64
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p MacOS-arm -j 8 -f

# Build only one package
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p Linux -t taskflow
```

## Step 2: Configure CMake

Common configure shape:

```bash
cmake -S <engine_root> -B <build_dir> \
  -G <generator> \
  -D3RD_PATH=<thirdparty_output_dir> \
  -DSKY_BUILD_TEST=OFF
```

Frequently useful switches:

- `-DSKY_BUILD_TEST=ON` — enable `ctest`
- `-DSKY_BUILD_EDITOR=ON` — build editor targets (Qt5 required)
- `-DSKY_BUILD_TOOL=ON` — build tooling targets
- plugin switches from `plugins/plugins.json`, such as `-DSKY_BUILD_BULLET=OFF`, `-DSKY_BUILD_PYTHON=ON`, `-DSKY_BUILD_GUIZMO=ON`
- platform/toolchain flags for Android or iOS when needed

## Plugin build configuration rules

Use `plugins/plugins.json` as the authoritative plugin compile-configuration entrypoint.

Each plugin entry defines:

- `name` — human-readable plugin key
- `dir` — subdirectory under `plugins/`
- `cmake_var` — cache variable used during configure/build selection
- `enabled` — repository default for that plugin switch
- `requires_editor` — optional editor-only gate

Repository flow:

1. `CMakeLists.txt` includes `cmake/plugin_switches.cmake` before `add_subdirectory(plugins)`
2. `cmake/plugin_switches.cmake` reads `plugins/plugins.json` and creates cache bools such as `SKY_BUILD_BULLET` and `SKY_BUILD_PYTHON`
3. `plugins/CMakeLists.txt` reads the same JSON and only calls `add_subdirectory(<plugin_dir>)` when the plugin's `cmake_var` is enabled
4. `cmake/thirdparty.cmake` consumes these switches to enable required third-party packages
5. Per-plugin `plugin.json` files are separate: they describe target dependency wiring after the plugin has already been included in the build

Working rules:

- Treat `plugins/plugins.json` as the source of truth for plugin default states
- Use `-D<cmake_var>=ON|OFF` at CMake configure time to override a plugin for the current build directory
- If `plugins/plugins.json` changes, `cmake/plugin_switches.cmake` refreshes the cached defaults on the next configure
- `requires_editor: true` means the plugin still needs its own `cmake_var` enabled, but it is skipped unless editor mode is active
- Do not document plugin enablement as runtime-only; this JSON controls configure/build inclusion
- Distinguish compile selection from runtime loading: compile selection comes from `plugins/plugins.json`, while runtime module lists come from `configs/modules_game.json` and `configs/modules_editor.json`

Current plugin defaults from `plugins/plugins.json`:

- `SKY_BUILD_GUIZMO=ON` with `requires_editor=true`
- `SKY_BUILD_PYTHON=OFF`
- `SKY_BUILD_XR=OFF`
- `SKY_BUILD_BULLET=ON`
- `SKY_BUILD_RECAST=ON`
- `SKY_BUILD_FREETYPE=ON`
- `SKY_BUILD_TERRAIN=ON`
- `SKY_BUILD_COMPRESSION=ON`
- `SKY_BUILD_PVS=ON`

Useful configure examples:

```bash
# Keep repository defaults from plugins/plugins.json
cmake -S <engine_root> -B <build_dir> \
  -G <generator> \
  -D3RD_PATH=<thirdparty_output_dir>

# Enable editor-only plugins and Python support
cmake -S <engine_root> -B <build_dir> \
  -G <generator> \
  -D3RD_PATH=<thirdparty_output_dir> \
  -DSKY_BUILD_EDITOR=ON \
  -DSKY_BUILD_GUIZMO=ON \
  -DSKY_BUILD_PYTHON=ON

# Disable optional runtime plugins for a leaner build
cmake -S <engine_root> -B <build_dir> \
  -G <generator> \
  -D3RD_PATH=<thirdparty_output_dir> \
  -DSKY_BUILD_BULLET=OFF \
  -DSKY_BUILD_RECAST=OFF \
  -DSKY_BUILD_COMPRESSION=OFF
```

Runtime note:

- Building a plugin does not automatically guarantee it is listed in runtime module configs
- `configs/modules_editor.json` controls editor module loading
- `configs/modules_game.json` controls game/runtime module loading
- Keep compile-time plugin switches and runtime module manifests in sync when enabling a new plugin for actual use

## Step 3: Build the engine

Canonical build command:

```bash
cmake --build <build_dir> --config Release --parallel <jobs>
```

For non-Android targets, runtime/library outputs default to `output/bin`.

## Step 4: Optional tests

Precondition: configure with `-DSKY_BUILD_TEST=ON` before building.

```bash
ctest --test-dir <build_dir> -C Release --output-on-failure
```

## Platform matrix

### Win32

- Third-party platform: `Win32`
- Third-party generator: `Visual Studio 17 2022`
- Recommended CMake generator: `Visual Studio 17 2022`
- Typical configure:

```bash
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p Win32 -j 8

cmake -S <engine_root> -B <build_dir> \
  -G "Visual Studio 17 2022" \
  -D3RD_PATH=<out> \
  -DSKY_BUILD_TEST=OFF

cmake --build <build_dir> --config Release --parallel 8
```

Notes:

- This is the most directly documented path in the repo README and the original plan doc.
- Windows-only dependencies like `dxcompiler` are resolved here.

### macOS (Intel)

- Third-party platform: `MacOS-x86`
- Third-party generator: `Xcode`
- Recommended CMake generator: `Xcode`

```bash
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p MacOS-x86 -j 8

cmake -S <engine_root> -B <build_dir> \
  -G "Xcode" \
  -D3RD_PATH=<out> \
  -DSKY_BUILD_TEST=OFF

cmake --build <build_dir> --config Release --parallel 8
```

Notes:

- Metal backend and Apple frameworks are configured on Darwin builds.
- Use this specifically for x86_64 macOS third-party output.

### macOS (Apple Silicon)

- Third-party platform: `MacOS-arm`
- Third-party generator: `Xcode`
- Recommended CMake generator: `Xcode`

```bash
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p MacOS-arm -j 8

cmake -S <engine_root> -B <build_dir> \
  -G "Xcode" \
  -D3RD_PATH=<out> \
  -DSKY_BUILD_TEST=OFF

cmake --build <build_dir> --config Release --parallel 8
```

Notes:

- Use this for arm64-native third-party artifacts.
- Do not reuse `MacOS-x86` third-party output for Apple Silicon builds.

### iOS

- Third-party platform: `IOS`
- Third-party generator: `Xcode`
- third-party bootstrap injects:
  - `CMAKE_TOOLCHAIN_FILE=<intermediate>/ios-cmake/ios.toolchain.cmake`
  - `PLATFORM=OS64`

```bash
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p IOS -j 8

cmake -S <engine_root> -B <build_dir> \
  -G "Xcode" \
  -D3RD_PATH=<out> \
  -DSKY_BUILD_TEST=OFF

cmake --build <build_dir> --config Release --parallel 8
```

Notes:

- The repo's third-party pipeline expects `ios-cmake` to exist under the intermediate workspace.
- iOS is arm64-oriented in the repository tooling.
- Xcode is required.

### Android

- Third-party platform: `Android`
- Third-party generator: `Ninja`
- third-party bootstrap injects:
  - `ANDROID_ABI=arm64-v8a`
  - `ANDROID_STL=c++_static`
  - `ANDROID_PLATFORM=android-31`
  - `CMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake`
- Required SDK env var: one of `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `ANDROID_SDK`
- Expected NDK version in the repo script: `27.0.12077973`

```bash
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p Android -j 8

cmake -S <engine_root> -B <build_dir> \
  -G Ninja \
  -D3RD_PATH=<out> \
  -DCMAKE_TOOLCHAIN_FILE=<ndk>/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-31

cmake --build <build_dir> --config Release --parallel 8
```

Notes:

- If the Android SDK/NDK env vars are missing, `python/third_party.py` fails.
- The repo also contains an Android Gradle project under `engine/launcher/android/` for launcher packaging.
- Native engine/library build and Android app packaging are related but not identical steps.

### Linux

- Third-party platform: `Linux`
- Third-party generator: `Ninja`
- Repository tooling also lists `Unix Makefiles` as an available configure generator on Linux

```bash
python3 python/third_party.py -i <int> -o <out> -e <engine_root> -p Linux -j 8

cmake -S <engine_root> -B <build_dir> \
  -G Ninja \
  -D3RD_PATH=<out> \
  -DSKY_BUILD_TEST=OFF

cmake --build <build_dir> --config Release --parallel 8
```

Notes:

- Linux is supported by the third-party script even though the top-level README platform table is older and omits it.
- If Ninja is unavailable, `Unix Makefiles` is a repository-supported alternative from the Python preset generator map.

## Optional build targets

Use these configure switches when the request needs more than the default launcher/runtime build:

- `-DSKY_BUILD_EDITOR=ON` — editor targets (Qt5 required)
- `-DSKY_BUILD_TOOL=ON` — tools such as asset/shader utilities
- `-DSKY_BUILD_TEST=ON` — test targets plus `ctest`

## Missing wrapper note

No repository wrapper script currently provides a one-command engine build flow.

When users ask for a one-command build flow, prefer the explicit `python/third_party.py` + `cmake` sequence shown above unless a supported wrapper is added back to the repository.

## Response style guidance

When answering with this skill:

- start by naming the target platform
- show bootstrap first, configure second, build third
- include test or packaging steps only when relevant
- mention platform constraints inline instead of burying them later
- do not suggest skipping `3RD_PATH` generation
