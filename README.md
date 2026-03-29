# SkyEngine

## Build

### Third-party strategy

SkyEngine supports two CMake-selectable third-party modes:

1. `LOCAL` (default): build/download third-party dependencies through vcpkg in workspace-local directories.
2. `PREBUILT`: use a prebuilt `3RD_PATH` tree produced by `python/third_party.py`.

Key CMake cache options:

| option | description |
|---|---|
| `SKY_THIRDPARTY_MODE` | `LOCAL` or `PREBUILT` |
| `SKY_THIRDPARTY_ROOT` | local dependency root for downloads/installed/cache |
| `SKY_THIRDPARTY_USE_PREBUILT` | in `LOCAL` mode, prefer reusable prebuilt binaries from local/CI cache |
| `3RD_PATH` | required in `PREBUILT` mode |

Examples:

```shell
# LOCAL mode: build/download packages to local workspace (no system install dir)
cmake -S . -B build -DSKY_THIRDPARTY_MODE=LOCAL -DSKY_THIRDPARTY_ROOT=.thirdparty

# PREBUILT mode: use prebuilt packages
cmake -S . -B build -DSKY_THIRDPARTY_MODE=PREBUILT -D3RD_PATH=build_3rd/Win32
```

### Build Third-party deps
```cmd
python3 python/third_party.py -p [platform]
```

* Third-party deps list:  cmake/thirdparty.json
* Third-party build args.

| args               | Description               |
|--------------------|---------------------------|
| -i, --intermediate | Third-party download/build path (default: `build_3rd/intermediate`) |
| -o, --output       | Third-party output root, effective `3RD_PATH` is `<output>/<platform>` |
| -e, --engine       | Engine path (default: current repository root) |
| -p, --platform     | Target Platform           |
| -c, --clean        | Clear build               |
| -t, --target       | Build Single Library      |

* Default third-party layout

When `-o/--output` is omitted, the script uses `build_3rd/` as the output root, and each platform gets a full isolated third-party tree:

```text
build_3rd/
  thirdparty_cache.cmake
  Win32/
  MacOS-x86/
  MacOS-arm/
  IOS/
  Android/
  Linux/
```

The effective `3RD_PATH` is the platform directory, for example:

- `build_3rd/Win32`
- `build_3rd/MacOS-arm`
- `build_3rd/Android`

After each successful third-party build, `build_3rd/thirdparty_cache.cmake` is refreshed. If `3RD_PATH` is empty, the repository CMake auto-loads this file and resolves `3RD_PATH` to the active platform directory.

* Examples

```cmd
:: Build all third-party libraries for Win32 into build_3rd/Win32
python3 python/third_party.py -p Win32

:: Build one library with custom root output; effective 3RD_PATH becomes <out>/Linux
python3 python/third_party.py -i <intermediate_path> -o <output_path> -e <engine_path> -p Linux -t taskflow
```

* Supported Platforms

| platform  | arch              |
|-----------|-------------------|
| Win32     | Window x86_64     |
| MacOS-x86 | MacOS with x86_64 |
| MacOS-arm | MacOS with arm    |
| IOS       | IOS arm64         |
| Android   | Android arm64-v8a |

### Build Engine Editor
```shell
cmake -S . -B build -G "Visual Studio 17 2022" -DSKY_THIRDPARTY_MODE=PREBUILT -D3RD_PATH=${path_to_3rd}
cmake --build build
```

If you use the default third-party layout, `${path_to_3rd}` should point to the platform subdirectory, for example `build_3rd/Win32`. You can also leave `3RD_PATH` unset and let CMake auto-load `build_3rd/thirdparty_cache.cmake`.

### CI build/publish/reuse of third-party packages

CI builds dependencies with `SKY_THIRDPARTY_MODE=LOCAL` and uses a workspace-local binary cache. The cache is:

1. restored before build for reuse,
2. populated by build jobs,
3. uploaded as a CI artifact on `main` for downstream reuse.

This keeps third-party package installation out of system directories and makes packages reusable across CI runs.

## Compile Shader
```shell
python .\assets\shaders\compileshaders.py
```

## Run Sample
```shell
Launcher.exe --module sample_module_name
```

![image](https://user-images.githubusercontent.com/35895395/195400282-ca50e99a-090b-4c52-a84c-d7e31a489e2f.png)

## Project Asset
```shell
AssetTool -e D:\Code\Engine\SkyEngine -p D:\Code\Engine\SkyEngine\sample\RDSceneProject
```
