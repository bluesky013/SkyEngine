## Context

The window/event layer is provided by SDL: `genetic/SDLPlatform` (init, message pump, clipboard, text input) and
`genetic/SDLWindow` (window creation via `SDL_CreateWindow`), wrapped by `windows/Win32Platform` +
`windows/Win32Window` and the macOS equivalents, which only add native-handle extraction. `Platform` and
`NativeWindow` (`engine/framework/include/framework/platform/PlatformBase.h`, `.../window/NativeWindow.h`) are the
seam used by the launcher, `GameApplication`, and the sandbox editor host.

Two facts drive this change:

- **SDL cannot create a second window on Windows here** (`SDL_CreateWindow` returns "invalid parameter"; the
  first window works). This blocks the editor's `WINDOW` viewport presentation.
- SDL also limits per-monitor DPI, native menus, and correct IME, all of which a first-class editor needs.

The RHI already consumes a native handle (`SwapChain::Descriptor.window` → `HWND` / `CAMetalLayer`), and the file
dialogs are already native (`Win32Platform` `comdlg32`). So the missing piece is a native window/event backend,
not a redesign of the abstraction.

## Goals / Non-Goals

**Goals**
- Provide native window/event backends (Win32, Cocoa) **behind the unchanged `Platform`/`NativeWindow`
  interfaces**, without SDL.
- Support multiple windows (fixes the editor's second window).
- Provide keyboard/mouse/text input events, the native handle, clipboard, and timing.
- Keep the launcher, editor host, and runtime source-compatible (no interface changes).

**Non-Goals**
- Mobile (Android/iOS) backends — keep SDL.
- Linux/X11.
- The editor menu bar (engine-drawn per `editor-ui-shell`); native menus only where the OS requires them
  (the macOS application/global menu) are a later, separate step.
- RHI changes (the native handle path is unchanged).

## Decisions

1. **Replace the backend, keep the interfaces.** `Platform` / `NativeWindow` (and `IWindowEvent` /
   `IKeyboardEvent`) are unchanged; only `Platform`'s implementation and the platform-specific window classes
   are rewritten to use native APIs. Nothing above the seam changes.
2. **Per-platform native backends.** Windows gets a real Win32 implementation (`CreateWindowEx`, a message loop,
   `WndProc`); macOS gets a Cocoa implementation (`NSWindow`/`NSView` with `CAMetalLayer`, an `NSEvent` run loop).
   The SDL backend is kept for mobile only (or removed from Windows/macOS builds).
3. **Event mapping.** Native events are translated to the existing engine events: window resize/focus/close →
   `IWindowEvent`; key down/up and text → `IKeyboardEvent` (`OnKeyDown`/`OnKeyUp`/`OnTextInput`). The mapping is
   per platform but the event types are shared.
4. **Native handle is the surface contract.** `NativeWindow::GetNativeHandle()` returns the `HWND` / `NSView`
   (backed by a `CAMetalLayer` on macOS); the RHI creates its surface from it. No RHI change.
5. **Clipboard and timing are native.** Win32 `OpenClipboard`/`GetClipboardData`/`SetClipboardData` and
   `QueryPerformanceCounter`; Cocoa `NSPasteboard` and `mach_absolute_time`.
6. **Text input / IME is phased.** Win32 `WM_CHAR` / Cocoa basic text first (ASCII), then Win32 `WM_IME_*` /
   Imm32 (CJK) and Cocoa `NSTextInputClient`. IME is the highest-risk item.
7. **Multi-window by construction.** The native backends create one window per `NativeWindow`; the message loop
   dispatches by window id. This removes the SDL single-window limitation the editor hit.

## Risks / Trade-offs

- **IME (CJK) is the hardest part.** Reimplementing text input + composition across Win32 and Cocoa is
  error-prone. Mitigation: phase it, start with ASCII `WM_CHAR`, and keep the event contract (`OnTextInput`) so
  editors do not change.
- **Shared layer.** `Platform`/`NativeWindow` are used by launcher/runtime too; a regression breaks everything.
  Mitigation: keep the interfaces frozen, land Win32 first, and validate against the existing hosts.
- **macOS cannot be built/validated in this environment.** Cocoa is written to the same contract and validated
  later on macOS.
- **Input fidelity.** SDL normalizes key codes and gamepad-style input; native mapping must match the engine's
  `IKeyboardEvent`/`ScanCode` expectations.
- **DPI / cursor / drag&drop.** Per-monitor DPI, cursor capture, and drag&drop must be reimplemented; keep the
  scope to what the editor needs first.

## Migration Plan

```
Phase 1  Win32 window + message loop + events + native handle (multi-window)   <- unblocks the editor
Phase 2  Win32 clipboard, timing (QPC), text input (ASCII via WM_CHAR)
Phase 3  Win32 IME (WM_IME_* / Imm32) for CJK
Phase 4  Cocoa backend: NSWindow/NSView + CAMetalLayer, NSEvent loop, clipboard,
         timing, text input/IME
Then     Remove SDL from Windows/macOS builds; keep it for mobile
```
Rollback: the SDL backend stays in the tree until the native one reaches parity, so the backend can be switched
back per platform.

## Open Questions

1. Keep SDL for mobile only, or also adopt native backends there later?
2. Phase order: Win32-first (recommended, unblocks the editor) or Cocoa-first?
3. Is the macOS application/global menu in scope here, or in a later change?
4. Which input surface must be exact (gamepad/haptics) versus editor-only needs?
