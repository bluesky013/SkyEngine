---
title: "Multi-Platform Support"
description: "Per-OS status of window/input/filesystem/process backends and RHI, with open TODOs and build caveats."
module: "framework"
updated: "2026-10-09"
---

## Overview

SkyEngine targets Windows, macOS, and Android (plus a planned iOS layer). This document is the single place
that tracks **what each platform backend implements today**, the **TODO gaps**, and the **cross-cutting
caveats** that repeatedly bite when adding platform-specific code. It is grounded in
`engine/framework/platform/*` and `engine/aurora/rhi/*`.

## Platform support matrix

| Concern | Windows | macOS | Android | iOS |
|---|---|---|---|---|
| Window | `platform/windows/Win32Window.cpp` | `platform/macos/CocoaWindow.mm` | `platform/android/AndroidWindow.cpp` | missing |
| Platform (app/services) | `platform/windows/Win32Platform.cpp` | `platform/macos/MacosPlatform.mm` | `platform/android/AndroidPlatform.cpp` | missing |
| Process | `platform/windows/Win32Process.cpp` | `platform/posix/PosixProcess.cpp` | (none) | missing |
| Asset/bundle filesystem | generic | generic | `platform/android/AndroidBundleFileSystem.cpp` | missing |
| RHI backend | DX12 (`aurora/rhi/dx12`), Vulkan | Metal (`aurora/rhi/metal`), Vulkan | Vulkan | Metal |
| Editor (`SandboxEditor` / `SandboxModule`) | built + validated | not set up | not set up | not set up |

Platform selection is done in `engine/framework/CMakeLists.txt` (Windows / macOS `+posix` / Android). There
is **no `platform/ios` directory** yet, so iOS is an RHI-only target for now.

The editor selects the backend from `--rhi <vulkan|dx12>` and otherwise passes `aurora::API::DEFAULT`
(`SandboxModule.cpp` `ParseApiArgs`), which the RHI resolves to a platform-available backend.

## Input layer

All window input is delivered as events in `engine/framework/include/framework/window/IWindowEvent.h` and
consumed by the editor through `IMouseEvent` / `IKeyboardEvent` (see `SandboxModule`).

### Mouse wheel event contract

`MouseWheelEvent` carries **cursor position in client space** plus a **signed `delta`**:

```cpp
struct MouseWheelEvent {
    WindowID winID;
    int32_t  x;     // cursor position (client space)
    int32_t  y;     // cursor position (client space)
    int32_t  delta; // wheel delta (positive = away from the user / scroll up)
};
```

The editor maps this to `UIPointerEvent{ action = WHEEL, x, y, wheelDelta = delta }`
(`SandboxModule::OnMouseWheel`), and UI panels hit-test the cursor and scroll. **A wheel event with
`x = 0` / no cursor cannot be routed** — this was the root cause of "wheel does nothing".

| Platform | Wheel status | Notes |
|---|---|---|
| Windows | Implemented | `WM_MOUSEWHEEL`, `ScreenToClient` for cursor, `GET_WHEEL_DELTA_WPARAM` for delta |
| macOS | **Stale / broken** | `CocoaWindow.mm::scrollWheel` still sets `y = scrollingDeltaY` (old convention); does not set cursor or `delta` |
| Android | **Unimplemented** | all mouse/wheel/key broadcasts are commented out in `AndroidWindow.cpp` |

### Event structs are shared across the exe and modules

`framework/window/*` event structs are **binary-shared** between `SandboxEditor.exe` and every module DLL
that listens (`SandboxModule.dll`, RHI backends, ...). Changing a field (as was done for `MouseWheelEvent`)
requires **rebuilding the exe and all dependent modules together**; otherwise the exe and module disagree on
the layout (undefined behavior, silently dropped input).

## Rendering backends and formats

- Backends: `aurora/rhi/vulkan`, `aurora/rhi/dx12`, `aurora/rhi/metal`. **GLES is not supported** — mobile
  uses Vulkan (Android) or Metal (iOS).
- ASTC: shipped in the texture cook pipeline (`ImageBuildConfig`/`AuroraImageBuilder`, block sizes 4/6/8).
  | Format | Vulkan | DX12 | Metal |
  |---|---|---|---|
  | ASTC 4x4 / 6x6 / 8x8 / 10x10 / 12x12 | `VK_FORMAT_ASTC_*` | `DXGI_FORMAT_UNKNOWN` (unsupported) | `MTLPixelFormatASTC_*` |

  DX12 has no native ASTC, so ASTC bundles (e.g. a `tex_mobile`/`ios_*` bundle) must not be used on a DX12
  desktop: keep ASTC in mobile bundles and use BC7/none for the desktop bundle.

## Build & packaging caveats

- **Rebuild exe + modules together** after touching shared structs/ABIs (`framework/window/*`,
  `framework/asset/AssetCommon.h`, event traits). Building only `SandboxModule` leaves a stale
  `SandboxEditor.exe`.
- **Configs**: engine defaults live in `configs/` (`modules_editor.json`, `modules_game.json`); a project
  may override them under `<project>/configs/`. Module `name` is the **DLL name** (e.g. `Aurora.Cook`,
  `AuroraShaderCompiler`), not the C++ class.
- **Cross-DLL singletons**: any process-wide registry shared between the exe and modules must derive from
  `sky::Singleton<T>` (stored in `Environment`). A plain function-local `static` gives each module its own
  copy and hides registrations (see `AGENTS.md`).
- Module `Init` order follows dependencies (`ModuleManager::LoadModules` loads + `Init`s each module in
  dependency order). E.g. `Aurora.Cook` registers builders before `SandboxModule` sets the workspace fs and
  loads build presets.

## TODOs

Priority order (highest first):

1. **Fix macOS mouse wheel** — update `CocoaWindow.mm::scrollWheel` to the new contract: set `x`/`y` from the
   event location converted to the view's client space, and `delta = scrollingDeltaY`. Cross-check the other
   `MouseWheelEvent` producers compile against the new fields.
2. **Implement Android input** — `AndroidWindow.cpp` has mouse/key/wheel broadcasts commented out; wire touch
   or mouse motion/button and (if applicable) wheel so the editor/game UI receives input.
3. **Decide iOS platform layer** — add `engine/framework/platform/ios` (window + platform services) or
   document iOS as RHI-only.
4. **`Platform::RevealInFileExplorer` on macOS** — currently only `Win32Platform` implements it; macOS falls
   back to the base no-op (`PlatformBase.cpp`). Implement the Cocoa `NSWorkspace` reveal.
5. **Editor on macOS/Android** — `SandboxEditor`/`SandboxModule` are only built and validated on Windows;
   verify the non-Qt editor builds and runs elsewhere (window, input, docking, DPI).
6. **Guard ASTC for DX12** — ensure the desktop bundle never selects an ASTC format (cook fails to
   `DXGI_FORMAT_UNKNOWN`), and surface a clear error instead of a silent bad product.

## References

- RHI backend rules and defaults: `engine/aurora/AGENTS.md`.
- Determinism constraints (fixed-width ints, endianness, FP): [Cross-Platform Determinism](../features/cross-platform-determinism.md).
- Asset cook pipeline and bundles: [Asset Pipeline](../features/asset-pipeline.md).
- Editor shell and panels: [Editor Framework Status](../editor/editor-framework-status.md).
