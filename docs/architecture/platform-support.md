---
title: "Multi-Platform Support"
description: "Per-OS status of window/input/filesystem/process backends and RHI, with open TODOs and build caveats."
module: "framework"
updated: "2026-11-01"
---

## Overview

SkyEngine has native Windows, macOS, Android, iOS and Linux platform layers, with HarmonyOS integration in progress. This document is the single place
that tracks **what each platform backend implements today**, the **TODO gaps**, and the **cross-cutting
caveats** that repeatedly bite when adding platform-specific code. It is grounded in
`engine/framework/platform/*` and `engine/aurora/rhi/*`.

## Platform support matrix

| Concern | Windows | macOS | Android | iOS |
|---|---|---|---|---|
| Window | `platform/windows/Win32Window.cpp` | `platform/macos/CocoaWindow.mm` | `platform/android/AndroidWindow.cpp` | `platform/ios/IOSWindow.mm` |
| Platform (app/services) | `platform/windows/Win32Platform.cpp` | `platform/macos/MacosPlatform.mm` | `platform/android/AndroidPlatform.cpp` | `platform/ios/IOSPlatform.mm` |
| Process | `platform/windows/Win32Process.cpp` | `platform/posix/PosixProcess.cpp` | explicit Unsupported | explicit Unsupported |
| Asset/bundle filesystem | native | native executable directory | APK asset filesystem | native app bundle |
| Timing | performance counter | Mach clock | monotonic nanosecond clock | Mach clock |
| RHI backend | DX12 (`aurora/rhi/dx12`), Vulkan | Metal (`aurora/rhi/metal`), Vulkan | Vulkan | Metal |
| Editor (`SandboxEditor` / `SandboxModule`) | main baseline | builds; native platform tests pass | not a mobile editor target | not a mobile editor target |

Platform selection is done in `engine/framework/CMakeLists.txt`: Windows, Darwin (Cocoa + POSIX),
Android (GameActivity), iOS (UIKit + POSIX), Linux (X11 + POSIX) and OHOS (XComponent).
The native service work does not establish GPU, signing, or packaged-game parity across platforms.

Hosts use `aurora::API::DEFAULT` unless an explicit backend is selected. DEFAULT maps to Metal on
Apple platforms and Vulkan elsewhere. Explicit backend/project preferences still override this default.

The consolidated [resolved-issue ledger](../../openspec/changes/archive/2026-10-16-complete-native-platform-services/validation.md#修复与补齐问题总表)
records root causes, fixes, evidence and remaining work across the native-service, UIScene and package phases.
Historical test counts and compile-only checks are kept separate from final package/runtime results.

## HarmonyOS integration in progress

Huawei native HarmonyOS has a separate `HarmonyOS` dependency platform, `harmonyos` CMake preset,
and `SKY_PLATFORM_OHOS` engine identity. It uses the same dependency set and pinned versions as Android,
rebuilt with the OHOS toolchain into a separate output. OHOS SDK libraries are handled independently.
The current compile baseline is Command Line Tools 26.0.0.865 / Native SDK 26.0.0.105,
API 26, arm64-v8a, C++20 and c++_shared. Older runtime API levels are not validated.

All 15 Android-matched dependencies and the OHOS root runtime build pass with the current SDK.
After SDK relocation, all 15 packages were actually rebuilt through the public third-party workflow
using `D:/Program Files/Huawei/sdk/default/openharmony/native`, with independent artifacts in
`build/harmonyos-thirdparty/HarmonyOS`. Canonical-SDK strict preflight reports **0 missing/stale,
0 output-missing**; this was not a cache-key rewrite. Package/version parity does not imply device
Python import or TLS runtime acceptance.
Core/Framework whole-archive links and the production Vulkan target pass; NEON is enabled in the root build.
Window/surface, rawfile, sandbox paths, HiLog, NativeVSync and the ArkTS/XComponent launcher are implemented.
The initial development HAP covered 21 AArch64 shared libraries, the shared STL and staged runtime resources;
that package's ELF dependency closure resolved against packaged libraries and SDK system stubs.
Signed installation and application startup pass on Pura 80 Pro / HarmonyOS 7.0.0.109 (API 26).
Its Maleoon 920 reports Vulkan 1.3.309 and lacks the surface/swapchain maintenance1 extensions.
The checked protocol now selects maintenance1 only when its dependencies and feature are available;
otherwise it uses core WSI with per-image present semaphores and deferred swapchain retirement.
The device passed three capability-negotiation tests and two 600-frame GPU clear runs. Blue/green
screenshots, one background/foreground cycle, and normal renderer shutdown were verified.
Core WSI final shutdown uses the queue-drain convention, which is not an independent present-completion
proof. The local, git-ignored `examples/HarmonyPhysics` acceptance project also passes cooked World/material loading,
indexed cube rendering and Jolt gravity/collision/stacking with 27 dynamic bodies on a static ground.
Its 12-second simulation reset and one background/foreground cycle pass on the same device.
Android's nine isolated terminal flows, OHOS's seven UI flows and native Chinese IME pass.
Clean formal APK/HAP packages were rebuilt through the public package command, audited without acceptance
probes, and restored on both phones. Android's nine and OHOS's eleven native libraries have complete
ELF dependency closure; all 80 OHOS rawfiles match staging byte for byte. Both physical scenes remain
visible after a same-process background/foreground cycle. Texture and missing-resource failure scenarios,
rotation and long-running GPU/presentation stress also remain open. Windows Framework and FrameworkTest
were rebuilt with the existing VS2022 cache; the complete run passes **270 tests**, including **17**
shared text-dialog state tests and **13** OHOS input policy tests executed on the desktop.
HiLog lives in `core/platform/ohos`, window/resource services in `framework/platform/ohos`,
and OHOS Vulkan surface setup in `aurora/rhi/vulkan/platform/ohos`, each in its existing target.
The approved host consolidation places `OHOSHost` in the existing Framework `platform/ohos` implementation.
A cross-DLL `Singleton` provides a stable callback receiver; its PIMPL owns sessions, the worker, NativeVSync,
the event queue, generation checks and stop/join. The project launcher retains N-API/XComponent registration
and supplies a factory that initializes `GameApplication` when invoked on the host worker.
`OHOSHost` remains independent of `OHOSPlatform`; the external N-API owner stops and joins the host before
releasing that owner. Platform windows consume shared native-window leases without retaining the SDK object
again. Native reference/unreference operations share one long-lived mutex, including delayed lease release.
This work adds no target and does not change the Android/iOS thread models. Tasks 6.9/6.10 record the
new host's API 26 build, restart and Windows regression, plus **30/30** device assertions covering SDK
failure paths and stale callbacks. A separate **65-second** run used real SDK VSync and XComponent
destruction/recreation, with 48 attachments, six background/foreground pairs and zero reported failures.
Stop balanced 48/48 Host window references, 1/1 applications and 1/1 VSync handles. RSS grew from
75092 to 92744 KiB by the 75-second sample and later reached 94428 KiB; file-descriptor access was denied.
This lightweight Host fixture does not run Renderer. Balanced explicit resources and a short observation
do not establish a leak-free process or GPU/present retirement; longer rendering stress remains open.
The API 26 Windows emulator image is prepared; host virtualization changes require a reboot,
and the x86 emulator still needs a separate ABI build. See the
[active checklist](../../openspec/changes/add-harmonyos-platform/tasks.md) and
[validation record](../../openspec/changes/add-harmonyos-platform/validation.md) for exact build coverage.

## Input layer

All window input is delivered as events in `engine/framework/include/framework/window/IWindowEvent.h` and
consumed by the editor through `IMouseEvent` / `IKeyboardEvent` (see `SandboxModule`).

### Native text dialogs

`Platform::ShowTextInputDialog` / `HideTextInputDialog` share `NativeTextInputOptions` and a callback
contract across Android, HarmonyOS and iOS. Options include text, placeholder, title, multiline,
password and a Unicode-scalar maximum length (zero is unlimited); multiline password requests are invalid.
These callbacks are separate from window key events and committed-text deltas.

| Backend | Native UI | Current verification |
|---|---|---|
| Android | Activity Dialog/EditText | Isolated phone fixture: 121 native + 20 UI checks, nine terminal flows pass; clean formal APK restored |
| HarmonyOS | ArkUI CustomDialogController + TextInput/TextArea | Final phone retest: seven flows, zero failures, 19 CHANGED / 5 CONFIRMED / 2 CANCELLED; native Chinese IME also verified |
| iOS | UIKit editor controller + UITextField/UITextView | Source and device-test runner provided; compile/runtime deferred because no Mac is available |

UI threads retain only a token and queue committed CHANGED snapshots; identical snapshots are deduplicated,
preedit is excluded, and one CONFIRMED/CANCELLED/ERROR is delivered on the engine owner thread.
Hide and session teardown drain accepted work. Terminal delivery retires the token before invoking user
code, allowing reopening; shutdown disables new requests first. Invalid CHANGED data is rejected, while
invalid terminal text or bridge-conversion failure completes with ERROR and the last valid snapshot.
Password contents and other input text are excluded from logs.

The shared state passes 17 desktop behavior tests. UIKit's 37 Python ownership/runner checks verify
source contracts only; they do not compile or execute UIKit. The user has deferred Mac validation,
which does not block the current Android/OHOS device work.

### Mouse wheel event contract

`MouseWheelEvent` carries **cursor position in client framebuffer pixels** plus a **signed fractional `delta`**:

```cpp
struct MouseWheelEvent {
    WindowID winID;
    int32_t  x;     // cursor position (client space)
    int32_t  y;     // cursor position (client space)
    float    delta; // positive scroll up; 120 per wheel notch, precise deltas retain fractions
};
```

The editor maps this to `UIPointerEvent{ action = WHEEL, x, y, wheelDelta = delta }`
(`SandboxModule::OnMouseWheel`), and UI panels hit-test the cursor and scroll. Missing real cursor
coordinates can hit-test the wrong UI region and make scrolling appear broken. Zero is a valid coordinate;
the defect was leaving coordinates unset rather than reporting the actual client position.

| Platform | Wheel status | Notes |
|---|---|---|
| Windows | Implemented | `WM_MOUSEWHEEL`, `ScreenToClient` for cursor, `GET_WHEEL_DELTA_WPARAM` for delta |
| macOS | Implemented + native tests | cursor converted from points to backing pixels; trackpad fractions retained |
| Android | Implemented + policy tests/SDK compilation | GameActivity input pump; fractional scroll, buttons, primary touch and keyboard |
| iOS | Touch mapping + native tests | primary contact maps to mouse events in framebuffer pixels; secondary contacts do not duplicate drags |

Cocoa/UIKit use the same view scale for resize, pointer input and text-input rectangles. Cocoa global
position/cursor services use a reversible desktop coordinate plane based on the primary display scale;
local client coordinates use each view's own backing scale. Native text input emits committed UTF-8,
while marked composition stays in the platform text system.

## Mobile lifecycle and startup

- `Platform::IsFrameActive()` gates application ticks while events continue to be received. Android requires
  a resumed app and live native surface; UIKit requires an active connected scene and a live window.
- `GetFrameEpoch()` changes across activity/surface transitions. `FrameClock` discards the previous timestamp
  on epoch change, including a stopped CADisplayLink interval where no inactive frame ever ran.
- Android keeps the engine window and WindowID across ANativeWindow loss/recreation and sends keyed
  `OnSurfaceChanged(window)` events. A null native handle means unavailable. RHI consumers must implement
  surface recovery separately; the platform layer does not create or destroy GPU resources.
- UIKit declares a single application scene. UIApplicationDelegate owns process startup/termination;
  UIWindowSceneDelegate connects the UI and forwards scene activity through `framework/platform/IOSSceneHost.h`.
  Only the bound scene may activate/deactivate frames. Application-wide notifications and stale/foreign
  scene callbacks cannot resume it. WillEnterForeground remains inactive; DidBecomeActive resumes it.
- UIKit creates its UIWindow with `initWithWindowScene:` and uses that scene's view geometry/display scale.
  Scene disconnection removes the native representation and exposes a null handle, while preserving the
  engine window ID and GameApplication. Reconnection recreates the surface without loading modules or
  reflecting types again. Explicit StopFrameLoop still cancels automatic resumption.
- Mobile work files use writable app data directories. Startup config lookup prefers that/project directory,
  then the bundle filesystem. Android packages `configs/` under APK assets with project overrides applied.
  A failed launcher initialization never starts the application tick loop.
- Android uses GameActivity **4.4.2** and `game-activity::game-activity_static`. Its official archive includes
  GameActivity, native app glue and GameTextInput; do not include those implementation sources again.
  The former 1.1.0 source distribution is incompatible with the current NDK compiler.
- APK paths use normalized logical separators; missing files and short reads fail explicitly. Buffered
  asset archives share one stream cursor for raw reads, skip, peek and position, including compressed assets.
- Worker synchronization uses a real counted semaphore on Android. NamedThread drains its worker before
  destroying that semaphore; Application drains asset work before withdrawing module services and releasing registries; loaded native code remains process resident.

### Event structs are shared across the exe and modules

`framework/window/*` event structs are **binary-shared** between `SandboxEditor.exe` and every module DLL
that listens (`SandboxModule.dll`, RHI backends, ...). Changing a field (as was done for `MouseWheelEvent`)
requires **rebuilding the exe and all dependent modules together**; otherwise the exe and module can disagree on
the type/representation or interface ABI (undefined behavior, silently dropped input). An int32-to-float change
can keep the same field size while still being incompatible.

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
  `Aurora.ShaderCompiler`), not the C++ class.
- **Cross-DLL singletons**: any process-wide registry shared between the exe and modules must derive from
  `sky::Singleton<T>` (stored in `Environment`). A plain function-local `static` gives each module its own
  copy and hides registrations (see `AGENTS.md`).
- Module `Init` order follows dependencies (`ModuleManager::LoadModules` loads + `Init`s each module in
  dependency order). E.g. `Aurora.Cook` registers builders before `SandboxModule` sets the workspace fs and
  loads build presets.
- Generated iOS games use static module factories and embed the Metal backend as a framework under
  `Frameworks/`; macOS deploys modules beside the executable under `Contents/MacOS`. The Core loader uses
  these bundle locations independently of the process working directory. Configs and products use the
  same production bundle filesystem roots.
- Android imports Slang under its actual compiler SONAME. Runtime-loaded companion libraries are listed
  by CMake and staged through an AGP variant task; compatibility aliases must not duplicate the compiler.
- AssetTool loads builder settings after modules register the builders, combines project config with
  deployed engine defaults, retains owning filesystem pointers, and returns failure for rejected batch
  products. Package commands propagate that failure.

### Template package smoke test

`python3 python/tests/create_package_smoke.py --dir <parent>` creates one lean game and prints the three
public package commands. The test keeps the actual renderer/game-module startup and disables optional
feature plugins. `python3 python/tests/run_asset_tool_package_tests.py` checks real successful and rejected
image cooks using the built AssetTool; the Python unit suite covers package boundaries and native library
models without requiring SDKs.

The 2026-10-15 run generated an arm64 Android debug APK, an arm64 unsigned iOS IPA and an x86_64 macOS app/ZIP.
The macOS app passed relocated startup/shutdown with default and explicit Metal. Android package signing,
alignment, assets and ELF dependencies were inspected; no Android device/emulator was available. The iOS
device-SDK app/framework layout, scene configuration and unsigned archive were inspected; no signing
identity/profile is installed. This is not real-device lifecycle or signed distribution acceptance.
See the [package validation record](../../openspec/changes/archive/2026-10-16-complete-native-platform-services/package-validation.md)
for exact SDK/minimum OS versions, commands, artifacts and remaining limits.

## TODOs

Priority order (highest first):

1. **RHI mobile lifecycle consumers** — consume surface-change notifications, recover native surfaces and
   add cancellation semantics for frames that acquired no drawable. These changes are outside the native
   platform service implementation.
2. **Native device acceptance** — exercise Android/iOS backgrounding, surface recreation/rotation, external
   keyboards and real IMEs on devices. Portable policies and SDK builds do not replace these cases.
3. **Release distribution** — add signed iOS archive/export and macOS signing/notarization, then validate
   physical devices and additional Apple architectures/minimum OS versions. The template smoke exports are unsigned.
4. **Guard ASTC for DX12** — ensure the desktop bundle never selects an ASTC format (cook fails to
   `DXGI_FORMAT_UNKNOWN`), and surface a clear error instead of a silent bad product.

## Regression validation

The `complete-native-platform-services` change contains executable cases for per-application clock and
resume epochs, semaphore permits, queued-signal teardown, asset-worker/module shutdown ordering,
startup config precedence, Android lifecycle/pointer/key/text/asset-read policies, UIKit lifecycle/touch
policies, and native Cocoa/UIKit services. FrameworkTest selects native `.mm` cases only for their OS.

```bash
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug --config Debug --parallel --target CoreTest FrameworkTest SandboxModule SandboxEditor
output/bin/Debug/CoreTest
output/bin/Debug/FrameworkTest
python3 -m unittest discover -s python/tests -v
```

Native Cocoa cases require a logged-in WindowServer session and skip without one. UIKit cases require
an UIApplication test host and an iOS simulator/device; FrameworkTest selects its checked-in UIKit
entry point and scene manifest on iOS. Tests start after UIKit connects and activates a real UIWindowScene.
Pure policy tests also run on desktop. A renderer-free simulator recipe uses the
installed SDK/runtime and existing GoogleTest sources:

```bash
python3 python/tests/run_ios_platform_tests.py --runtime iOS-18-6 --work-dir /tmp/sky-ios-tests
```

The recipe creates and removes its own temporary simulator, or accepts `--udid` and restores the
existing simulator's boot state. It preserves compile/link/test logs and verifies the native test
process exit marker.

Android native cases are SDK-gated and need a device/emulator to execute. Build the Android Framework
using the Prefab package supplied by the generated Gradle project. With `SKY_BUILD_TEST=ON`, the Android
FrameworkTest executable uses a headless test entry that supplies the GameActivity native glue hook
and defaults to native contracts, Android policies and shared FrameClock cases. It does not drive an
Activity or substitute for installed-APK lifecycle acceptance. The iOS test entry defaults to iOS
platform cases; both mobile entries honor an explicit GoogleTest filter.

Apple requires scene lifecycle adoption for apps built with the latest SDK beginning with iOS 27;
see the [official migration guide](https://developer.apple.com/documentation/uikit/transitioning-to-the-uikit-scene-based-life-cycle?changes=_4).
The source migration uses APIs available since iOS 13. This workspace currently has an iOS 18.5 SDK and
iOS 18.6 simulator runtime; those runs do not establish an iOS 27 SDK build or runtime result.

Exact executed results and compile-only limits are recorded in
[native platform validation](../../openspec/changes/archive/2026-10-16-complete-native-platform-services/validation.md).

## References

- RHI backend rules and defaults: `engine/aurora/AGENTS.md`.
- Determinism constraints (fixed-width ints, endianness, FP): [Cross-Platform Determinism](../features/cross-platform-determinism.md).
- Asset cook pipeline and bundles: [Asset Pipeline](../features/asset-pipeline.md).
- Editor shell and panels: [Editor Framework Status](../editor/editor-framework-status.md).
