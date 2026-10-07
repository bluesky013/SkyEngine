## 1. Action registry (core)

- [x] 1.1 `EditorActionRegistry` (cross-DLL `Singleton`): `EditorAction{id,label,icon,group,menu,submenu,order,menuOrder,enabled,invoke}`; `Add`/`Clear`/`Find`; `GetToolbarActions` (group != "") and `GetMenuActions` (menu != "") sorted.
- [x] 1.2 Module registers the built-in actions (New/Open/Save/Close/Quit, Undo/Redo, Reset Layout, Demo/About, Play/Pause/Stop) with menu/group/icon/enabled.
- [x] 1.3 `EditorShell::InvokeAction(id)` + `RefreshActions()`; remove the per-command shell handlers.

## 2. Toolbar + menus + shortcuts from the registry

- [x] 2.1 `ToolBar` widget (icon-only items, group separators, disabled dim, hover tooltip).
- [x] 2.2 Menus built from the registry (`menu`/`menuOrder`/`submenu`); nested submenus in `MenuBar`; text-measured labels.
- [x] 2.3 Keyboard shortcuts (Ctrl+S/W/Z/Y, F5/Shift+F5) invoke actions by id.
- [x] 2.4 Retire Window/Tools menus; `View > Windows` submenu + `Reset Layout`.
- [x] 2.5 Tests: toolbar hit + enabled state; Ctrl+S/W and F5 via registered actions.

## 3. UI scaling (single theme source)

- [x] 3.1 `UiTheme` (`UiMetrics`/`UiFonts`) is the single dimension source; `UiMetrics::Scale` + `MakeDarkTheme(scale)` scale all metrics and fonts.
- [x] 3.2 Convert hub, dialogs, config panel, and widget popups to read `metrics`/`fonts` (no hard-coded pixel layouts); remove the `UiPx`/`UiFont` shims.
- [x] 3.3 `editor.uiScale` preference; effective scale = system DPI × preference; applied to the editor and the hub.
- [x] 3.4 Menu/toolbar/status heights are theme metrics (`menuBarHeight`/`toolbarHeight`/`statusBarHeight`) so the chrome scales with DPI.
- [x] 3.5 Tests: `UiMetrics::Scale` doubles dimensions + fonts.

## 4. Reset Layout correctness

- [x] 4.1 `LayoutModel::CaptureDefault()` snapshots the built-out default; `ResetToDefault` restores it.
- [x] 4.2 Test: close down to one panel, `ResetToDefault` restores the full arrangement.

## 5. Docs

- [x] 5.1 `docs/editor/editor-toolbar.md`, `docs/editor/ui-sizing-and-scaling.md` (+ README index, status doc).
