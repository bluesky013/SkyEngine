## 1. Win32 backend: window + message loop + events (Phase 1)

- [x] 1.1 Implement `Win32Platform` without SDL: `Init` (register the window class), the message pump (`PeekMessage`/`DispatchMessage`), clipboard, and `QueryPerformanceCounter` timing
- [x] 1.2 Implement `Win32Window` without SDL: `CreateWindowEx` (per-window), `GetNativeHandle` = `HWND`, `DestroyWindow`, resize
- [x] 1.3 Map `WM_*` to the engine events: resize/focus → `IWindowEvent`; key down/up (VK→`ScanCode`) → `IKeyboardEvent`; `WM_CHAR` → `OnTextInput`; mouse → `IMouseEvent`
- [x] 1.4 Verify multi-window: `SandboxEditor` now creates its second window (two swapchains: 1280x720 + 320x180) — unblocks the editor `WINDOW` viewport
- [x] 1.5 Configure the build so Windows links the native backend and not SDL (`platform/windows/*` only; `3rdParty::sdl` dropped for Windows)

## 2. Win32 shell services (Phase 2)

- [x] 2.1 Clipboard (`OpenClipboard`/`GetClipboardData`/`SetClipboardData`) behind `Platform`
- [x] 2.2 Timing (`QueryPerformanceCounter`/`Frequency`) behind `Platform`
- [x] 2.3 Text input (ASCII) via `WM_CHAR` → `OnTextInput`

## 3. Win32 IME (Phase 3)

- [ ] 3.1 IME composition via `WM_IME_*` + Imm32: start/stop composition, preedit/commit, candidate positioning
- [ ] 3.2 Verify CJK input into a focused text field (editor console / text field)

## 4. Cocoa backend (Phase 4)

- [ ] 4.1 `MacosPlatform` + `MacosWindow`: `NSWindow`/`NSView` with a `CAMetalLayer`, `NSEvent` run loop, clipboard (`NSPasteboard`), timing (`mach_absolute_time`)
- [ ] 4.2 Map `NSEvent` to `IWindowEvent`/`IKeyboardEvent`; text input via `NSTextInputClient`
- [ ] 4.3 Verify multi-window + surface creation on macOS

## 5. Cleanup and validation

- [ ] 5.1 Remove SDL from the macOS build too, then delete `platform/genetic/*` (SDLPlatform/SDLWindow/GeneticPerfManager) and `3rdParty::sdl` entirely (Windows is already SDL-free)
- [ ] 5.2 Validate launcher + editor host on Windows; validate on macOS
- [ ] 5.3 Re-run the editor `WINDOW` viewport presentation (`editor-renderer` 4.4) now that a second window works (swapchains verified; visual pending)
