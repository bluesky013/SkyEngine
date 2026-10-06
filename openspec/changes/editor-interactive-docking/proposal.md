## Why

The sandbox editor already has a headless `LayoutModel` (split/tab/ratio/close/JSON) and an `EditorShell`
that renders it, but the layout is **read-only chrome**: splitters cannot be dragged, tabs cannot be
moved between areas, panels cannot be torn out or re-docked, there is no layout save/reset, and the top
chrome is a flat "Show/Hide" toolbar with no menu bar or status bar. Users cannot adjust the workspace,
so the design's "docking provided by `editor-layout`" requirement and the reserved
`editor-floating-docking` capability are unrealized.

## What Changes

- **Interactive in-window docking** in `EditorShell`: draggable splitters (resize), tab drag with
  reorder/tabify, drag-to-dock with a transient drop-zone highlight (center = tabify, edge = split in
  that orientation), and a per-tab close button. All edits mutate `LayoutModel`.
- **Layout model extensions**: a floating-panel state (panel id + geometry) and a dock-target/drop-zone
  operation so the shell can express "drop here", kept toolkit- and render-independent.
- **Floating tear-out** (`editor-floating-docking`): dragging a tab/panel out of the main window creates
  a `NativeWindow` with its own `sky::ui` context + `ClientViewport`/swapchain; dragging it back re-docks
  it into the main tree. `EditorRenderer` generalizes its existing preview-window path to host N floating
  windows on the one Aurora device.
- **Top chrome**: a real engine-drawn **menu bar** (File / Edit / View / Window / Tools / Help) replacing
  the flat item row, plus a **status bar** (project, engine version, RHI, fps, mode, selection count).
  **BREAKING** for the existing `EditorShell` toolbar API (`ToolBar` items are replaced by menus).
- **Layout persistence wired**: auto-save the arrangement (including floating geometry) under
  `GetUserConfigPath()` and restore on startup; `View > Reset Layout` restores the default arrangement.
  `LayoutPersistence` is currently test-only.
- **Per-window input routing**: keyboard/text input is routed by `winID` to the owning `UIContext`;
  pointer drags are owned by a **global drag coordinator** using OS-level pointer capture plus screen
  coordinates, so tear-out/re-dock works even when the pointer leaves the source window. `SandboxModule`
  currently ignores `winID` and broadcasts one shell.
- **Multi-window render host**: generalize `EditorRenderer`'s single `GuiPaintFn` seam (`EditorRenderer.h`)
  into a window-aware host that paints each `UIContext` with its own `UIRenderer`, and registers the shared
  viewport content target in every window's texture registry.
- **Window lifecycle across backends**: implement interactive tear-out/re-dock on **Win32 in v1**;
  non-Win32 backends stay docked (a pre-created hidden-window pool, required by the Vulkan-instance
  creation order, is documented as the extension path). Broadcast `OnWindowClose` on all backends
  (currently a no-op in `SDLWindow::Dispatch`) and add an OS window move notification for floating
  geometry.
- **v1 scope exclusions**: layout undo/redo (not routed through `CommandController`), interactive tear-out
  on non-Win32 backends, and live cross-monitor DPI changes (per-window DPI is set at window creation
  only).

## Capabilities

### New Capabilities
- `editor-floating-docking`: floating panel tear-out/re-dock bound to native windows, including
  multi-window rendering of panels, the native-window creation-order constraint, cross-backend close
  broadcast, and the window-move notification used for geometry write-back.

### Modified Capabilities
- `editor-layout`: add a floating-panel state and a dock-target/drop-zone operation; extend persistence
  to store floating geometry, accept OS window-move write-back, and be auto-saved/restored per user.
- `editor-ui-shell`: add interactive splitters/tab-drag/drag-to-dock/close buttons, per-window input
  routing, replace the flat tool bar with an engine-drawn menu bar, and add a status bar.
- `editor-application`: drop the "toolbars" wording (this change removes the flat tool bar; the menu bar
  remains, specified by `editor-ui-shell`).

## Impact

- `engine/sandbox/core`: `layout/LayoutNode.h`, `layout/LayoutModel.{h,cpp}` (floating state, dock
  targets), `layout/LayoutPersistence.{h,cpp}` (floating geometry, auto-save), `layout/DefaultPanels.cpp`.
- `engine/sandbox/shell`: `EditorShell.{h,cpp}` plus new `sky::ui` elements (menu bar, status bar,
  splitter handles, transient drop-zone highlight); per-panel view registry that survives `Rebuild`;
  per-window input routing through `UIEventRouter`.
- `engine/sandbox/render`: `EditorRenderer.{h,cpp}` generalized from one `GuiPaintFn` into a multi-window
  host (main + N floating windows; per-window `UIContext`/`UIRenderer`; shared content target registered
  in each window), still on one device.
- `engine/sandbox/module`: `SandboxModule.cpp` wiring (load/save layout, floating window lifecycle, chrome,
  `winID → UIContext` routing, global drag coordinator).
- `engine/framework/window` + platform backends: `NativeWindow`/`IWindowEvent` window-move notification;
  Win32 pointer capture; SDL/macOS `OnWindowClose` broadcast (currently a no-op in `SDLWindow`).
- Tests: extend `LayoutModelTest`, `LayoutPersistenceTest`, `DefaultPanelsTest`; add floating/dock
  target coverage and a headless `EditorShellTest` for the view registry and gesture hit-testing. Update
  `docs/editor/editor-framework-design.md` status.
