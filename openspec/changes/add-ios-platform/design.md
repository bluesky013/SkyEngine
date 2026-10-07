## Context

iOS shares the Apple toolchain with macOS but not the UI framework: macOS uses AppKit (`NSApplication`,
`NSWindow`); iOS uses UIKit (`UIApplication`, `UIViewController`, `UIView`). The engine's platform abstraction
(`NativeWindow` / `PlatformBase` / `IWindowEvent` / `IKeyboardEvent` / `IMouseEvent`) is platform-agnostic, so
iOS is an additional backend behind unchanged interfaces — analogous to the existing Win32 and Cocoa backends.

`Aurora.Metal` already targets Apple (`CAMetalLayer` is available on iOS), and the Metal swapchain consumes a
`CAMetalLayer` handle, so the iOS window backend must expose the same handle shape.

## Goals / Non-Goals

**Goals**
- A UIKit window/event backend behind the existing `NativeWindow`/`PlatformBase` interfaces.
- `GetNativeHandle()` returns the content view's `CAMetalLayer` (same contract as macOS).
- An iOS launcher entry (`UIApplicationMain` + AppDelegate) that runs the engine `Application`.
- CMake builds iOS from `platform/ios/*` with UIKit/QuartzCore/Metal frameworks.

**Non-Goals**
- Vulkan-on-iOS (MoltenVK) — optional, only if required later.
- iOS app signing / App Store packaging specifics (document, don't formalize here).
- Replacing the event interfaces with native touch types.

## Decisions

### D1. UIKit backend mirrors the Cocoa one
`IOSPlatform : PlatformBase` and `IOSWindow : NativeWindow` in `engine/framework/platform/ios/`, structured like
`platform/macos` (AppKit types hidden in `.mm`, handles as `void*`). The content view's `layerClass` is
`CAMetalLayer`; `GetNativeHandle()` returns it.

### D2. Event loop
`PollEvent` drains the UIKit run loop. Engine frames are driven by a `CADisplayLink` (or `NSTimer`) ticking the
`Application` loop; `UIApplicationMain` owns the outer loop via an AppDelegate. No `NSApplication` usage.

### D3. Input mapping
- Touch: `touchesBegan/Moved/Ended/Cancelled` map to the engine mouse/touch events (position in points →
  backing pixels for coordinates that drive rendering). Consider a first-class touch event if game input needs
  multi-touch; otherwise map single-touch to `Mouse*`.
- Soft keyboard: `UIKeyInput`/`UITextInput` on the view → `IKeyboardEvent` (`OnKeyDown/Up`, `OnTextInput`).
- DPI/scale: `UIScreen.scale` / `nativeScale` → `GetDpiScale()`.

### D4. CMake split (Darwin vs iOS)
`engine/framework/CMakeLists.txt`: keep `Darwin` for macOS (`platform/macos/*`), add an iOS branch (detected via
`CMAKE_SYSTEM_NAME STREQUAL "iOS"` under the iOS toolchain) compiling `platform/ios/*` and linking
`-framework UIKit -framework QuartzCore -framework Metal -framework Foundation`. Reuse the
`posix` process backend.

### D5. Launcher
`engine/launcher/ios/` with `main.mm` (calls `UIApplicationMain`) and an `AppDelegate`/`ViewController` hosting
the engine view; the engine `Application` is started from the app lifecycle (didFinishLaunching).

### D6. ARC for the iOS backend
Like the Cocoa backend, the iOS `.mm` backend SHALL be built with ARC (`-fobjc-arc` on those sources) and follow
the same handle/bridge conventions.

## Risks / Trade-offs

- [Touch vs mouse model] -> start by mapping single touch to `Mouse*`; add a touch event type only if needed.
- [Lifecycle/backgrounding] -> iOS suspends the app; `PollEvent` must handle `applicationWillResignActive`
  (pause) and resume without leaking the drawable.
- [Third-party for iOS] -> runtime packages need iOS cross-builds (or `3RD_PATH` iOS output); only `ios-cmake`
  tooling exists today.
- [RHI on mobile] -> swapchain/drawable resizing on rotation; MSAA/blit paths must be validated on device.
- [No device/CI here] -> verification requires an iOS toolchain + simulator/device.

## Open Questions

- Touch input: map to `Mouse*` or introduce a dedicated touch event?
- Display-link vs timer for the frame tick.
- Whether MoltenVK (Vulkan on iOS) is in scope.
