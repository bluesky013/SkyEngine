## Context

The sandbox editor already has a headless `LayoutModel`/`LayoutNode` (`core/layout/`: split/tab/panel tree,
`SplitPanel`/`Tabify`/`ClosePanel`/`SetRatio`/`ResetToDefault`, versioned JSON), an `EditorShell` that
renders it into one `UIContext`, a test-only `LayoutPersistence`, and an `EditorRenderer` that already
demonstrates a second native window (preview + its own swapchain/command buffer, "scheme C"). What is
missing: interaction, floating/tear-out, save/restore, menu/status chrome. Three current-code facts shape
the design:

- **Input is global and window-less** — `SandboxModule` ignores `winID`, `UIEventRouter` capture is
  per-context, and Win32 took no OS pointer capture.
- **The render seam is single-window** — `GuiPaintFn`/`SetGuiSource` paint one context; the preview
  surface is a hardcoded singleton.
- **Window lifecycle is Win32-only** — the project is now single-backend (Win32); SDL has been removed
  entirely (no macOS backend). Windows created after `Instance::Init` must remain graphics-capable, which
  constrains runtime tear-out.

Layering (`AGENTS.md`): `core/layout` stays headless (no `sky::ui`/Aurora); `shell` may use `sky::ui`;
`render` may use Aurora; `SandboxModule` wires. No engine→plugin or editor→runtime edge is added.

## Goals / Non-Goals

**Goals:** in-window docking (splitter drag, tab reorder/tabify, drag-to-dock drop zones, close);
floating tear-out/re-dock on real OS windows sharing one device; engine-drawn menu + status bar;
per-user layout auto-save/restore/reset; per-window input routing; cross-backend window lifecycle.

**Non-Goals:** named workspaces (`editor-workspaces`), a general action registry (`editor-actions`),
multi-viewport scene content/PIE/content browser, floating tab groups (v1 = one panel per window),
free-form docking, layout undo/redo integration (not routed through `CommandController`), interactive
tear-out on non-Win32 backends in v1, and live cross-monitor DPI changes (per-window DPI is set at window
creation only).

## Decisions

1. **Model owns docking semantics; shell owns gestures.** `LayoutModel` gains dock (target + position),
   float, dock-floating, and geometry operations; the shell hit-tests, tracks drags, and calls the model.
   Keeps layout headless-testable.

2. **Floating panels are a model list, not tree nodes.** `FloatingPanel{panelId,x,y,w,h,active}` in a
   `std::vector` beside `root`; a panel is docked **xor** floating. Floating has no sibling ratios or
   collapse, so a tree node kind would create degenerate trees and harder JSON.

3. **Drop zone = center + four edges.** `DockPosition{Center,Left,Right,Top,Bottom}`; center tabifies,
   edges split on that side with the right orientation and normalize the parent. A drop that hits no window
   (empty screen space) leaves the panel floating. Matches UE/VS.

4. **Gesture zones are discrete elements, not one full-area overlay.** `UIEventRouter` captures the
   **topmost hit** on `DOWN` regardless of the handler's return (`UIEventRouter.cpp:101-108`), so one big
   transparent overlay would starve panels. Use thin **splitter-handle** elements on seams + existing tab
headers; the **drop-zone highlight** exists only during a drag. All hit-test geometry is device pixels
(paint scale applies only at paint). The reusable geometry — child rects, splitter bands, ratio-from-drag,
and dock-position resolution — lives in the headless `core` helper `DockInteraction` (UI-toolkit-free,
unit-tested), so the shell only translates gestures into core calls. *(Alternative: make the router capture
only on `HANDLED` — touches `engine/ui`, deferred.)*

5. **Window-aware render host, one device.** Generalize the preview singleton into a
   `FloatingSurface{NativeWindow, ClientViewport, CommandBuffer, UIContext, UIPaintContext, UIRenderer}`
   list; replace `GuiPaintFn(ctx,w,h)` with a window-aware host (`GuiPaintFn(windowId,ctx,w,h)` /
   `IEditorWindowHost`). Each surface owns its own `UIRenderer`; shared content targets (viewport
   placeholder) are registered per window. `Tick` renders every surface and submits together on the one
   device. Floating windows paint UI only (no per-window scene pass).

6. **Shell-owned view registry.** Panel views live in `panelId → unique_ptr<UIElement>`; docking/tear-out
   detaches (`RemoveChild` clears `parent`, `UIElement.cpp:21-42`) and re-attaches to the target
   `UIContext`. `Rebuild` rebuilds only the dock tree and must not destroy floating/preserved views.

7. **Persistence v2, per user.** Layout JSON gains a `floating` array; loader tolerates v1 and unknown
   ids. Auto-save is coalesced to end-of-frame and writes only **committed** state: a drag in progress does
   not mark the model dirty; the drag end (pointer `UP`) does. The renderer is authoritative for live
   geometry and writes it back into the model (`OnWindowMove`/resize), so the model stays the single
   serialized source. `View > Reset Layout` = `ResetToDefault` + clear floating.

8. **Minimal in-shell menus + status bar.** `MenuBar`/`Menu` built from state (View = panel visibility +
   Reset Layout; Window = Reset Layout; File/Edit/Tools/Help minimal); `StatusBar` reads existing services
   (project, version, RHI, fps, mode, selection). Full action registry deferred. **BREAKING**: flat
   `ToolBar` row removed.

9. **Per-window input: `winID` routing + a global drag coordinator.** Keyboard/text route by `winID` to the
   owning `UIContext` (fall back to the active window when the id is `0`); non-drag pointer events route
   the same way. Drags that may cross windows (tab reorder, drag-to-dock, tear-out, re-dock) are owned by a
   coordinator that takes OS pointer capture on drag start (Win32 `SetCapture` + `GetCursorPos`) and tracks
   **screen** coordinates. `winID` routing alone cannot see motion outside a window or own a cross-context
   drag.

10. **Window lifecycle.** Single backend (Win32); SDL has been removed from the project. Interactive
    tear-out/re-dock is implemented on Win32; other backends are out of scope until a native one is added.
    `OnWindowClose` broadcasts (Win32) and closing a floating window never requests app exit; `OnWindowMove`
    drives geometry write-back. DPI scale is set per window at creation, not from one global snapshot.
    Invariant: the main window is created before any floating window.

**Capability ownership:** `editor-layout` = model semantics/persistence; `editor-ui-shell` = input,
interaction, chrome; `editor-floating-docking` = native-window behavior and its lifecycle; `editor-application`
= platform services (menu bar reference only).

## Risks / Trade-offs

- **[Views across contexts]** → one shell-owned registry; explicit detach/attach; no `UIContext`-owned
  lifetime assumed.
- **[Rebuild churn]** → full rebuild only on structural change; ratio/geometry updates mutate bounds only.
- **[Cross-window drag]** → OS capture + screen space; without it tear-out/re-dock silently fails at the
  window edge.
- **[Non-Windows backend]** → SDL has been removed; a future native backend must add window creation
  (before device init, or a pre-created hidden-window pool) and close/move broadcasting to reach parity.
- **[Per-window GPU state]** → isolated `UIRenderer`s and per-window target registration; a multi-swapchain
  scene render is out of scope.
- **[Renderer↔shell coupling]** → the single window-aware host seam is the only crossing; the shell never
  owns `NativeWindow`, the renderer never owns panel views. Extract `EditorWindowHost` if it grows.
- **[Layering]** → `core/layout` stays UI/Aurora-free; enforce by review against `AGENTS.md`.

## Migration Plan

Build/test-green increments: (1) framework windows (`OnWindowMove`, pointer capture, close broadcast);
(2) model floating/docking + tests; (3) shell registry + discrete gesture elements + interaction;
(4) window-aware render host + per-window `UIRenderer`; (5) `winID` routing + drag coordinator;
(6) tear-out/re-dock (Win32) + lifecycle; (7) persistence + chrome + per-window DPI; (8) docs. SDL is
removed from the project (Win32-only).
Rollback: revert; the v2 loader is forward-compatible (older readers ignore `floating`).
