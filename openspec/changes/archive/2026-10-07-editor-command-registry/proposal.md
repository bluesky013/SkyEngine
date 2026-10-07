## Why

The Sandbox editor's commands were hard-coded per surface: the toolbar wired per-command shell handlers, the
menus hard-coded lambdas + handlers, and keyboard shortcuts called handlers directly. There was no plugin
extension point, no single source of truth for a command's label / icon / enabled state, and adding a command
meant editing several places. Separately, UI sizing/scaling was ad-hoc: many views hard-coded pixel layouts and
the shell chrome (menu / status bar) did not scale with DPI, so the UI looked inconsistent at non-100% scaling.

## What Changes

- Add a toolkit-agnostic **`EditorActionRegistry`** (cross-DLL `Singleton`) of
  `EditorAction{id,label,icon,group,menu,submenu,order,menuOrder,enabled,invoke}`. Plugins contribute through
  `EditorExtension::Register()`.
- The **toolbar** and **menus** are built from the registry; **keyboard shortcuts** invoke actions by id
  (`EditorShell::InvokeAction`). Remove the per-command shell handlers.
- Toolbar: icon-only buttons (SVG → UI texture), full-height group separators, hover tooltips.
- Retire the top-level **Window** / **Tools** menus; move `Reset Layout` into **View** and the docking-window
  toggles into a **View > Windows** submenu (requires nested submenu support in `MenuBar`).
- **UI scaling**: `UiTheme` is the single source (`UiMetrics` / `UiFonts`); `MakeDarkTheme(scale)` scales every
  metric + font size; every view reads `metrics` / `fonts` (no hard-coded pixel layout). The menu / toolbar /
  status heights are theme metrics so the chrome scales with DPI. Effective scale = system DPI × the
  `editor.uiScale` preference; the Project Manager hub scales too.
- `LayoutModel::CaptureDefault()` snapshots the built-out default so `View > Reset Layout` restores the whole
  arrangement (previously it collapsed to the single initial panel).

## Capabilities

### New Capabilities
- `editor-command-registry`: a plugin-extensible action registry driving the toolbar, menus, and shortcuts.
- `editor-ui-scaling`: a single-theme-source, DPI-aware UI sizing/scaling model.

### Modified Capabilities
- *(none — additive; the shell's per-command handler API is removed in favor of the registry.)*

## Non-goals

- Command palette, data-driven keybinding maps, per-focus/context enablement, localization, menu-item gray-out.
- Command objects / undo transaction naming at the action layer (undo/redo still come from `CommandService`).
- User-customizable / hideable toolbars, tool modes, or combined Play controls.

## Impact

- `engine/sandbox/core`: new `editor/core/extension/EditorActionRegistry`; `LayoutModel::CaptureDefault`.
- `engine/sandbox/shell`: registry-driven `EditorShell` (toolbar + menus + shortcuts), `ToolBar` widget,
  `MenuBar` submenus + tool-measured labels, `UiTheme` single-source metrics, all views themed.
- `engine/sandbox/module`: registers the built-in actions; icon textures via `EditorIconRegistry`/`UiIconBuilder`-style SVG raster.
- Resources: `engine/sandbox/resources/icons/*.svg`.
- Docs: `docs/editor/editor-toolbar.md`, `docs/editor/ui-sizing-and-scaling.md`.
