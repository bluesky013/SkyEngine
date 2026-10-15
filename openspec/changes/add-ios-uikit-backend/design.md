> Historical design: application-wide activity notifications, global-screen geometry and implied drawable
> recovery below are not the current implementation contract. The single-scene lifecycle, stable window
> identity and actual coverage are documented in
> [complete-native-platform-services](../archive/2026-10-16-complete-native-platform-services/validation.md).
> GPU surface recovery is still outside that implementation. Original tasks/archive state is unchanged.

## Context

iOS and macOS share the Apple toolchain but not the UI framework: macOS uses AppKit (`NSApplication`,
`NSWindow`); iOS uses UIKit (`UIApplication`, `UIViewController`, `UIView`). The engine's platform abstraction
(`NativeWindow` / `PlatformBase` / event interfaces) is framework-agnostic, so the iOS backend is a sibling of
`platform/macos`, structured the same way (AppKit/UIKit types confined to `.mm`, native handles as `void*`, ARC
for the `.mm`). The Metal swapchain consumes a `CAMetalLayer`, so the iOS window exposes the same handle shape.

## Goals / Non-Goals

**Goals**
- `IOSPlatform` + `IOSWindow` in `engine/framework/platform/ios`, behind the existing interfaces.
- `GetNativeHandle()` returns the content view's `CAMetalLayer`.
- Frame driving via `CADisplayLink`; touch + soft keyboard mapped to engine events.
- Buildable for iOS (simulator/device) via a dedicated CMake branch.

**Non-Goals**
- The launcher/`main.mm`/AppDelegate and iOS third-party (see `add-ios-platform`).
- Vulkan-on-iOS (MoltenVK).
- Full multi-touch API (start with single-touch -> `Mouse*`; see open questions).

## Decisions

### D1. Class layout (mirrors Cocoa)
`IOSPlatform : PlatformBase`, `IOSWindow : NativeWindow`. AppKit/UIKit types kept in `.mm`; native handles are
`void*`. ARC enabled for the iOS `.mm` files.

### D2. Bootstrap & view
`UIApplicationMain` (in the launcher, `add-ios-platform`) creates an AppDelegate -> `UIViewController` whose view
is `SkyIOSView : UIView` with `+ (Class)layerClass { return [CAMetalLayer class]; }`. `GetNativeHandle()` returns
`(CAMetalLayer *)view.layer`. `NativeWindow::Create` is defined for the iOS TU (guarded by `SKY_PLATFORM_IOS`).

### D3. Frame loop
UIKit owns the run loop; the engine cannot block in `Application::Mainloop`. The view controller owns a
`CADisplayLink` whose callback drives the engine frame. This requires a **clean seam** in the shared
`Application`: the desktop `Mainloop` (which loops on `PollEvent` + tick) SHALL NOT be used on iOS. Instead the
`CADisplayLink` tick calls the per-frame entry (`Application::Loop` / a `Tick` hook) directly. `IOSPlatform::PollEvent`
becomes a no-op with respect to the run loop (events arrive via UIKit callbacks). This is the main divergence from
macOS/desktop.

### D4. Input
- Touch: `touchesBegan/Moved/Ended/Cancelled` -> single-touch mapped to the existing `Mouse*` events (position in
  points multiplied by screen scale for render-space coordinates). Multi-touch is a follow-up (see open questions).
- Soft keyboard: the view implements `UIKeyInput`/`UIResponder`; text/key events map to `IKeyboardEvent`
  (`OnKeyDown/Up`, `OnTextInput`).
- DPI: `UIScreen.mainScreen.nativeScale` (or `scale`) -> `GetDpiScale()`.

### D5. Lifecycle
`applicationWillResignActive`/`didEnterBackground` pause the display link; `willEnterForeground`/`didBecomeActive`
resume. On background the drawable must be released to avoid a leaked `CAMetalDrawable` (the swapchain already
handles slot release; ensure the display link stops).

### D6. CMake
`engine/framework/CMakeLists.txt`: branch on `CMAKE_SYSTEM_NAME STREQUAL "iOS"` -> compile `platform/ios/*` and
link `-framework UIKit -framework QuartzCore -framework Metal -framework Foundation`; reuse `platform/posix`.
Keep the macOS branch on `platform/macos/*` only when not iOS so the two never mix. Note: an iOS toolchain sets
`CMAKE_SYSTEM_NAME` to `iOS`, **not** `Darwin`, so the existing `Darwin` branch will not match iOS — an explicit
`iOS` branch is required.

### D7. Initialization order
The AppDelegate creates the `UIViewController`/`SkyIOSView` first (app launch), then the engine `Application`
starts and `NativeWindow::Create` returns an `IOSWindow` wrapping that view. `GameApplication::PreInit` (which
creates the `NativeWindow`) therefore runs **after** the view exists; the RHI then samples the layer size. Do not
create the window before the UIKit view is ready.

## Risks / Trade-offs

- [Frame ownership] -> `CADisplayLink` vs the desktop `PollEvent` model needs the explicit `Mainloop` seam (D3);
  `Application` must not enter its own loop on iOS.
- [Init order] -> window creation before the view/RHI is a hard failure (D7).
- [Input model] -> mapping touch to `Mouse*` loses multi-touch; a first-class touch event may be needed for games.
- [Backgrounding] -> drawable/resource lifetime across suspend/resume; test on device/simulator.
- [Build validation] -> simulator is easiest; device needs signing (see `add-game-project-build` 8.x).

## Open Questions

- Dedicated touch event vs single-touch -> `Mouse*`.
- `CADisplayLink` frame pacing vs fixed-step accumulation for deterministic/game loop.
- Whether the iOS backend shares code with macOS via a `platform/apple` base.

