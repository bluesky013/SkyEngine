## 1. Framework window lifecycle and input primitives

- [x] 1.1 Add a window-move notification (`IWindowEvent::OnWindowMove`) and broadcast it from the Win32 window backend. (SDL removed from the project.)
- [x] 1.2 Broadcast `IWindowEvent::OnWindowClose` on the non-Windows path. (SDL backend removed; now Win32-only, no SDL path remains.)
- [x] 1.3 Add OS-level pointer capture (`NativeWindow::SetPointerCapture` + `GetGlobalCursorPosition`; Win32 `SetCapture`/`ReleaseCapture` + `GetCursorPos`) exposed through the framework API. (SDL removed.)
- [x] 1.4 Confirm and document the main-window-candidate invariant (main window created before any floating window) for `Win32Platform::SetMainWindow`.
- [x] 1.5 Add a cursor API (`NativeWindow::SetCursor(StandardCursor)`; Win32 `LoadCursor` `IDC_SIZEWE`/`IDC_SIZENS`/`IDC_SIZEALL`/`IDC_ARROW`) so UI elements can request a resize/move cursor on hover. Win32-only (SDL removed).

## 2. Layout model: floating state and docking

- [x] 2.1 Add `DockPosition { Center, Left, Right, Top, Bottom }` and a `FloatingPanel { panelId, x, y, width, height, active }` type in `engine/sandbox/core/include/editor/core/layout/`.
- [x] 2.2 Add a floating-panel set to `LayoutModel` (accessors, add/remove/find, `CollectFloating`) with docked/floating exclusivity.
- [x] 2.3 Implement `DockPanel(panelId, targetPanelId, DockPosition)` (center = tabify; edges = split on the requested side with the correct orientation and normalization).
- [x] 2.4 Implement `FloatPanel(panelId, geometry)` (remove from tree, add to floating set, collapse) and `DockFloatingPanel(panelId, targetPanelId, DockPosition)` (remove from floating set, insert into tree).
- [x] 2.5 Implement `SetFloatingGeometry(panelId, rect)` and make `SetDefault`/`ResetToDefault`/`Clear` clear the floating set; reject drops onto the same panel.
- [x] 2.6 Extend `ToJson`/`FromJson` to `version: 2` with a top-level `floating` array, tolerant of v1 input and unknown panel ids.
- [x] 2.7 Add headless, UI-toolkit-free docking geometry helpers in `core` (`DockInteraction`: child rects, splitter bands, ratio-from-drag, dock-position resolution) so the shell stays thin; covered by `DockInteractionTest`.

## 3. Layout model tests

- [x] 3.1 Extend `engine/sandbox/test/LayoutModelTest.cpp` with docking tests for each `DockPosition` (center and all four edges) and self-drop rejection.
- [x] 3.2 Add float/re-dock tests asserting docked/floating exclusivity, empty-area collapse, geometry write-back, and `ResetToDefault` clearing floating.
- [x] 3.3 Extend `engine/sandbox/test/LayoutPersistenceTest.cpp` with a v2 floating round-trip and a v1 (no floating) load.

## 4. Persistence wiring

- [x] 4.1 Add an auto-save path (default `<user-config>/editor_layout.json`) and a load that applies a saved layout when present and valid. (Wired in `SandboxModule::BuildEditor`; verified restore via log.)
- [x] 4.2 Mark the layout dirty on committed mutations only (dock/close/float/ratio, and geometry at drag end) and auto-save coalesced to end-of-frame, including on shutdown. (`EditorShell::ConsumeLayoutDirty` + `SandboxModule` Tick/Shutdown; covered by `EditorShellTest`.)

## 5. Shell: view registry and in-window interaction

- [x] 5.1 Introduce a `panelId -> unique_ptr<UIElement>` view registry owned by the shell; `Rebuild` rebuilds only the dock tree and preserves views for floating/preserved panels (detach/attach via `RemoveChild`/`AddChild`).
- [x] 5.2 Add discrete splitter-handle elements on the seams (computed in device pixels during `EditorShell::Layout`) and a transient drop-zone highlight element created only during a drag; do not use a single full-area overlay (the router captures the topmost hit on `DOWN`).
- [x] 5.3 Implement splitter drag: capture pointer, map drag delta to `LayoutModel::SetRatio`, re-layout without a full rebuild.
- [x] 5.4 Add tab drag with reorder + drag-to-tabify and a drop-zone highlight (center + four edges).
- [x] 5.5 Add a per-tab close affordance that closes the panel through the model and rebuilds on structural change.
- [x] 5.6 Add a headless `EditorShellTest` target (link `EditorShell` + `UI` + `EditorCore` + `Core`, like `EditorCoreTest`) covering the view registry across `Rebuild` and splitter/tab hit-testing; extract hit-test math into a testable helper if needed.
- [x] 5.7 Interaction polish: splitter seam line + hover grab band + centered grip + drag highlight; tab drag ghost; and requesting the resize cursor on splitter hover (via `NativeWindow::SetCursor`) are all done. Remaining: a tab-header hover state (optional cosmetic).

## 6. Shell: menu bar and status bar

- [x] 6.1 Add a `MenuBar`/`Menu` `sky::ui` element (labeled top-level menus, popups, enabled state, item actions).
- [x] 6.2 Build menus from state: File/Edit/View/Window/Tools/Help with View = panel visibility + Reset Layout and Window = Reset Layout; remove the flat `ToolBar` item row.
- [x] 6.3 Add a `StatusBar` element (project, engine version, RHI, fps, mode, selection count) reading existing services.
- [x] 6.4 Lay out header (menu bar), content area, and footer (status bar) in `EditorShell::Layout`/`Rebuild` and paint them through the theme.

## 7. Render: multi-window host

- [x] 7.1 Replace the single-window `GuiPaintFn`/`SetGuiSource` seam with a window-aware host that paints a given window's `UIContext` (`GuiPaintFn(surfaceId, ctx, w, h)`; main = 0). Callers updated in `EditorRenderer` + `SandboxModule`.
- [x] 7.2 Generalize the preview singleton (`previewWindow`/`previewViewport`/`previewTarget`) into a floating-surface list (`NativeWindow` + `ClientViewport` + command buffer + `UIContext` + `UIPaintContext` + `UIRenderer`), acquired/released per the backend creation strategy (on-demand or hidden pool), sharing the one device.
- [x] 7.3 Give each floating surface its own `UIRenderer` and register any shared content target (viewport placeholder) in every window's texture registry.
- [x] 7.4 Render each surface's UI context each `Tick` and submit all command buffers together with the main frame.

## 8. Input routing and global drag coordinator

- [x] 8.1 Route input by `winID`: `SandboxModule::AcceptWindowEvent` learns the primary window id from the first event and accepts only its events (or id-less ones as a fallback); keyboard/pointer routed accordingly, text input now wired (`DispatchText`).
- [x] 8.2 Implement the drag coordinator's core: on mouse DOWN take OS pointer capture (`NativeWindow::SetPointerCapture`) and release on UP, so motion keeps arriving when the cursor leaves the window (`GetGlobalCursorPosition` available for screen-space resolution).
- [x] 8.3 Use the coordinator for tab reorder, drag-to-dock, tear-out, and re-dock so drags survive leaving the source window. (Reorder/dock + capture done; tear-out/re-dock resolution completes with Group 9.)

## 9. Floating tear-out/re-dock and lifecycle

- [x] 9.1 Implement tear-out (drag a tab outside the main window) creating or reusing a floating window, and re-dock (drag over a dock area) moving the panel view between contexts.
- [x] 9.2 Implement interactive tear-out/re-dock on Win32 (on-demand window creation). Disable tear-out on non-Win32 backends in v1 (no half-working path) and leave the placement seam for a later pre-created hidden-window pool.
- [x] 9.3 Handle floating-window close → re-dock via `OnWindowClose` on every backend, and floating geometry write-back via `OnWindowMove`/resize.
- [x] 9.4 Per-window DPI: `NativeWindow::GetDpiScale` (Win32 `GetDpiForWindow`), surface scale captured at creation (`EditorRenderer::SurfaceDpiScale`), and applied per surface in `EditorShell::PaintSurface` (theme swapped to the surface's scale). Live cross-monitor changes remain a Non-Goal.

## 10. Wiring and startup

- [x] 10.1 Wire `SandboxModule::BuildEditor` to load the saved layout (or fall back to the default arrangement) and to save on shutdown. (Done with Group 4.)
- [x] 10.2 Route input binders through the `winID -> UIContext` map and the global drag coordinator. (Done with Group 8: `AcceptWindowEvent` + OS pointer capture.)
- [x] 10.3 Confirm `View > Reset Layout` and status-bar values are bound to the same services used at startup. (`Reset Layout` calls the live `LayoutModel`; status bar now fed `SetStatusInfo(project, rhi, mode)` from the module.)
- [x] 10.4 On startup, materialize floating windows for panels in the restored layout's floating set (and on shutdown, tear down surfaces/contexts in the correct order).
- [x] 10.5 Investigate the apparent window/surface size mismatch (window ~1037x607 vs swapchain 1278x712). Resolved as **not a bug**: the swapchain already matches the physical window client (measured 1278x712; DPI 125%), and the earlier clipping/status-bar-missing was an artifact of a DPI-unaware screenshot tool capturing a cropped region. A DPI-aware capture shows the full layout incl. the status bar (`SkyEngine Edit RHI: - sel: 0 0 fps`). No code change required.

## 11. Verification and docs

- [x] 11.1 Build `SandboxEditor` (`cmake --build build --config Release --target SandboxEditor`) and run the `EditorCoreTest` and `EditorShellTest` targets.
- [ ] 11.2 Manually verify: splitter resize, tab reorder/tabify, drag-to-dock (center + edges), close tab, panel input pass-through, float/re-dock across windows, floating window close, geometry persistence, Reset Layout, menu bar, status bar, per-window DPI.
- [x] 11.3 Update `docs/editor/editor-framework-design.md` (implementation status) and `docs/editor/editor-framework-status.md`.
