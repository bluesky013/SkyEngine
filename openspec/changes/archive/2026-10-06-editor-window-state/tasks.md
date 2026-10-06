## 1. Window backend

- [x] 1.1 `NativeWindow`: add `GetPosition`/`SetPosition` (default no-op/false).
- [x] 1.2 `Win32Window`: update the live client size on `WM_SIZE`; implement `GetPosition` (`GetWindowRect`) / `SetPosition` (`SetWindowPos`).

## 2. Editor host

- [x] 2.1 `EditorApplication`: load `<user-config>/skyengine/editor_window.json` before creating the window, apply the saved size/position, and save the live geometry on destruction.
- [x] 2.2 Skip saving for bounded/dev runs (`--frames N`); ignore malformed JSON and implausible sizes.

## 3. Docs and validation

- [x] 3.1 Add an editor "Persistence" doc section covering window state. (docs §3.11.)
- [x] 3.2 Manual verify: resize + move, close normally, relaunch restores; force-kill does not lose the previous state. (Verified: saved file updates on close and restores on launch.)
