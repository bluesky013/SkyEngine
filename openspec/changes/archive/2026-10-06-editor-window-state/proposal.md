## Why

The editor does not remember its main-window size or position across sessions, so users re-resize and
re-position the window on every launch. The panel layout (`editor_layout.json`) and preferences
(`editor-preferences.json`) already persist per user; the window geometry should too.

## What Changes

- Persist the **main window size and position** to `<user-config>/skyengine/editor_window.json` on graceful
  exit, and **restore** them on the next launch.
- Make the native window report its **live** client size and its screen position (today
  `GetWidth/GetHeight` return the initial descriptor and there is no position accessor).
- Bounded/dev runs (`--frames N`) SHALL NOT overwrite the saved geometry.

## Capabilities

### New Capabilities
- `editor-window-state`: persist and restore the editor main-window geometry.

### Modified Capabilities
- `editor-application`: the main window is created with the restored geometry and its geometry is saved on
  exit.

## Impact

- `engine/framework`: `NativeWindow` gains `GetPosition`/`SetPosition`; `Win32Window` updates its live client
  size on `WM_SIZE` and implements position get/set.
- `engine/sandbox/app`: `EditorApplication` loads geometry before creating the window and saves it on
  destruction.
- Non-goal / follow-up: restoring the maximized/minimized state, fullscreen, and validating that a restored
  position is on an attached monitor.
