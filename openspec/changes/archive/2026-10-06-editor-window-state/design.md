## Context

The editor host (`EditorApplication`) creates the native main window in `PreInit` from fixed defaults
(1280x720). `NativeWindow::GetWidth/GetHeight` return the descriptor (initial) size, and there is no
position accessor; the Win32 backend already receives `WM_SIZE`/`WM_MOVE` but only broadcasts them. The
panel layout and preferences already persist per user, so window geometry should live alongside them.

## Goals / Non-Goals

**Goals:** remember and restore the main window size and position per user; keep it invisible when there is
no saved state.

**Non-Goals:** maximize/minimize/fullscreen state, DPI-change handling, off-screen clamping, and
per-project window state (this is a per-user editor preference, not per project).

## Decisions

1. **Where.** A small `<user-config>/skyengine/editor_window.json` (`{width,height,x,y}`), next to
   `editor_layout.json` / `editor-preferences.json` (see `editor-preferences-dialog`). Owned by the app
   (which creates the window), read before window creation and written on graceful exit.
2. **Live size.** `Win32Window` writes the client size back into its descriptor on `WM_SIZE`, so
   `GetWidth/GetHeight` report the live size with no extra event plumbing.
3. **Position.** Add `NativeWindow::GetPosition/SetPosition` (default no-op/false); the Win32 backend uses
   `GetWindowRect`/`SetWindowPos`. Position uses window-rect coordinates on both ends, so it round-trips.
4. **When to save.** In `~EditorApplication` (after a graceful close), not on force-kill. Bounded dev runs
   (`--frames N`) skip saving so they do not clobber the user's geometry.
5. **Validation.** Reject implausible saved sizes (< 320x240) and ignore malformed JSON, falling back to the
   defaults.

## Risks / Trade-offs

- **[Off-screen restore]** a saved position from a disconnected monitor can open off-screen; deferred
  (follow-up). Mitigation later: clamp to the virtual screen.
- **[Maximized]** a maximized window saves its maximized client size and restores as a large normal window;
  acceptable for now, maximize-state is a follow-up.
- **[Force-kill]** no save occurs; only graceful exit persists (documented).
