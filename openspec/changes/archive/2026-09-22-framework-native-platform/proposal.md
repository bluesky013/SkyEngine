## Why

The window/event layer is currently provided by **SDL** (`engine/framework/platform/genetic/SDLPlatform` +
`SDLWindow`, wrapped by `Win32Platform`/`Win32Window` and `Macos*`). SDL now blocks the editor: creating a
**second** window fails on Windows (`SDL_CreateWindow` returns "invalid parameter"; the first window works, and
it is unrelated to flags), which blocks the editor's `WINDOW` viewport presentation. SDL also limits the native
behaviours a first-class editor needs — per-monitor DPI, native menus, and correct IME.

The design already calls for a **thin native platform layer** with the editor drawn by the engine (the model used
by UE's `GenericApplication` and Blender's GHOST). The `Platform` / `NativeWindow` abstraction for this already
exists; what is missing is a native backend instead of the SDL one.

## What Changes

- Implement native backends **under the unchanged `Platform` / `NativeWindow` interfaces** (no redesign):
  - **Windows**: `CreateWindowEx` + a Win32 message loop; `WM_*` mapped to `IWindowEvent` / `IKeyboardEvent`;
    `GetNativeHandle` = `HWND`; clipboard via `OpenClipboard`; timing via `QueryPerformanceCounter`; text
    input/IME via `WM_IME_*` / Imm32.
  - **macOS**: `NSWindow` / `NSView` (backed by `CAMetalLayer`); `NSEvent` run loop; clipboard via
    `NSPasteboard`; timing via `mach_absolute_time`; text input/IME via `NSTextInputClient`.
- Multi-window support (fixes the editor's second window); the RHI already creates its surface from the native
  handle (`SwapChain::Descriptor.window`), so no RHI changes are required.
- Keep the interfaces stable so `launcher`, the editor host, and the runtime are unaffected.
- Phase the work: (1) Win32 window + events + native handle; (2) clipboard / timing / text input (ASCII);
  (3) IME (CJK); (4) Cocoa.
- Mobile (Android/iOS) keeps SDL for now; Linux is out of scope.

## Capabilities

### New Capabilities
- `native-platform-backend`: a window/event/input backend implemented with native OS APIs (Win32, Cocoa) behind
  the existing `Platform` / `NativeWindow` interfaces, without SDL — covering window creation (multi-window),
  the event loop, keyboard/mouse/text input (`IWindowEvent`/`IKeyboardEvent`), the native handle for the RHI,
  clipboard, and timing.

### Modified Capabilities
<!-- None: the Platform / NativeWindow contracts are unchanged. -->

## Impact

- `engine/framework/platform/windows` (native Win32 window/events, replacing the SDL-backed one).
- `engine/framework/platform/macos` (native Cocoa window/events).
- `engine/framework/platform/genetic` (SDL backend kept for mobile only, or removed from Win/mac builds).
- `engine/framework/src/platform/PlatformBase.cpp` + build config (backend selection).
- Dependency surface: SDL removed from the Windows/macOS editor builds.
- Unblocks the editor's `WINDOW` viewport presentation (the second window) and future native menus / IME.
