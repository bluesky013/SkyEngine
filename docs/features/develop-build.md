# Develop builds

Develop is an optimized configuration with symbols and engine assertions. It is intended for everyday editor/game work where Debug is too slow. Debug, Release, RelWithDebInfo and MinSizeRel remain available.

## Policy

| Configuration | Optimization | Symbols | Engine assertions (AUTO) | GPU validation (AUTO) | MSVC CRT / prebuilt libraries |
|---|---|---|---|---|---|
| Debug | Off | Yes | On | On | Debug |
| Develop | On | Yes | On | Off | Release |
| RelWithDebInfo | On | Yes | Off | Off | Release |
| Release | On | Toolchain default | Off | Off | Release |
| MinSizeRel | Size | Toolchain default | Off | Off | Release |

MSVC Develop uses `/O2 /Ob2 /Zi /DNDEBUG`, `/MD`, and linker debug information. It does not define `_DEBUG` or enable `/RTC1`. Other compilers inherit their RelWithDebInfo compile/link flags. Optimized stepping can skip lines and optimize away local variables; use Debug when that obstructs debugging.

`SKY_BUILD_DEVELOP` identifies the configuration. `SKY_DEVELOP` independently enables developer facilities, and is enabled by Sandbox/Tool builds or an explicit `-DSKY_DEVELOP=ON`. Game Develop presets enable it explicitly. Resource names default to this facilities intent. Vulkan requests debug utilities for resource naming even when validation is off, if the driver supports that extension.

Three cache policies accept `AUTO`, `ON`, or `OFF`:

- `SKY_ASSERTIONS` controls `SKY_ENABLE_ASSERT` independently of `NDEBUG`. Failed engine invariants print the expression/file/line and terminate; disabled assertions still evaluate their expressions once, matching the existing contract. Standard-library `assert` remains controlled by `NDEBUG`.
- `SKY_GPU_VALIDATION` controls the editor/runtime host validation request. The requested layers must be installed to run validation.
- `SKY_RESOURCE_NAMES` controls `SKY_ENABLE_RESOURCE_NAME`. This affects public descriptor layouts: rebuild all participating modules with the same setting.

`SKY_USE_TRACY` remains a separate profiling option. Diagnostic cache settings apply to all configurations in that build tree. Return a policy to `AUTO` to restore its configuration defaults; use a separate build tree if two policies must coexist. Do not mix DLLs or static libraries from different configurations or settings.

## Windows editor

Close SandboxEditor before configuring/building. Bootstrap dependencies through `python/third_party.py` and pass the resulting Win32 directory as `3RD_PATH`; an existing cache can retain its actual dependency location.

```powershell
cmake --preset windows-develop
cmake --build --preset windows-develop --parallel 3 --target SandboxEditor SandboxModule AssetTool ShaderCookTool
output/bin/Develop/SandboxEditor.exe --project D:/Projects/MyGame/MyGame.skyproj
```

The Windows preset reuses `cmake-build-debug`; that directory name does not select the configuration. `windows-debug` and `windows-release` build presets use the same tree. Presets require CMake 3.21+; direct configure/build remains supported by the existing minimum CMake version:

```powershell
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Develop --parallel 3 --target SandboxEditor SandboxModule
```

For an existing `build/SkyEngine.sln`, regenerate with `cmake -S . -B build` and reopen the solution. A previous virtual-folder collision could make Visual Studio omit Core/Framework projects and then report missing `core/Develop/Core.lib`; their test folders now live under `Foundation/Tests`. Do not repair generated solution references by hand.

To enable validation explicitly, configure with `-DSKY_GPU_VALIDATION=ON` and rebuild; reset with `-DSKY_GPU_VALIDATION=AUTO`. Develop executables, modules and PDB files are under `output/bin/Develop`.

## Game projects and other generators

New game projects include `<platform>-develop` configure/build presets. Existing projects can update their native CMake/presets from the template or select Develop directly. On Windows the launcher template supplies `mainCRTStartup` for its GUI subsystem, matching its `main(argc, argv)` source entry.

From a generated project's `native` directory:

```powershell
$env:SKY_ENGINE_ROOT = 'D:/Code/Engine/SkyEngine'
cmake --preset windows-develop -D3RD_PATH=D:/Code/Engine/SkyEngine/build_3rd/Win32
cmake --build --preset windows-develop --parallel 3
```

The game executable and modules are in `<Project>/build/windows/bin/Develop`; the CMake tree is `<Project>/build/windows-develop`. Original Release publishing presets remain unchanged. `package_project.py package` still implements the existing Android/Apple publishing flows; this change does not add a Windows installer or distribution format.

Single-configuration generators select Develop at configure time, for example `cmake -S . -B cmake-build-develop -G Ninja -DCMAKE_BUILD_TYPE=Develop`. Multi-configuration generators select it with `--config Develop`. Root presets cover Windows, Linux and macOS; game presets also cover iOS and Android. These presets require each platform's SDKs; Windows is the native validation platform for this change.

AssetTool/ShaderCookTool automatic discovery checks Develop, Debug, Release, RelWithDebInfo, then the flat legacy directory. `cook-shaders --tool <path>` still takes precedence. Tools must stay with their matching sibling modules and runtime libraries.

```powershell
python python/package_project.py cook-shaders --project D:/Projects/MyGame --target vulkan
```
