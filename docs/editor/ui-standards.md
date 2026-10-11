# Editor UI Standards (Sandbox Shell)

Single source of truth for editor chrome (toolbars, menus, buttons, panels). If you add
editable UI, follow this so the editor does not end up "east one, west one".

Related docs: `editor-toolbar.md`, `ui-sizing-and-scaling.md`, `editor-framework-design.md`
(§3.5 menus, §3.2 layout), `reflection-widget-framework.md`, and the forward plan
`ui-layout-and-chrome-plan.md` (target architecture + phased migration).

## 1. One declarative action source (mandatory)

All chrome is driven by **`EditorActionRegistry`** (`editor/core/extension/EditorActionRegistry.h`),
a cross-DLL `Singleton`. Register one action per command:

```cpp
EditorAction a;
a.id = "asset.save";          // stable id: toolbar + menu + shortcut all key off this
a.label = "Save";
a.icon = "save";              // icon id (see §3); omit for text-only
a.group = "File";             // toolbar group (empty = not on the toolbar)
a.menu = "File"; a.menuOrder = 0;
a.order = 30;
a.accelerator = "Ctrl+S";     // data-driven shortcut (comma-separates alternatives)
a.scope = "global";           // "" / "global" = main window; a dialog pushes its own scope
a.enabled = [] { return true; };
a.invoke = [] { /* ... */ };
EditorActionRegistry::Get()->Add(std::move(a));
```

Actions are keyed by `(scope, id)`; `EditorScopeStack` holds the active scope (top of stack).
Build chrome only through **`ActionBarBuilder`** (`BuildToolBarItems` / `BuildMenus` /
`MatchAccelerator`), which filters the registry by the active scope. Do **NOT** hand-build
toolbars/menus per window, and do **NOT** hard-code shortcuts in a `switch` — each action declares
its `accelerator` and `EditorShell::DispatchKey` resolves it through `ActionBarBuilder`.

## 2. Use the shared widgets (mandatory)

- Toolbar → `editor/shell/widgets/ToolBar` (reads `GetToolbarActions()`, icon-only + tooltip).
- Menu bar → `editor/shell/widgets/MenuBar` (reads `GetMenuActions()`; two levels max today).
- Panels/dialogs → `UiSkin` painters (`DrawPanel`, `DrawSectionHeader`, `DrawRow`, `DrawField`,
  `DrawToolItem`, `DrawCheckbox`, `DrawSlider`, `DrawPopup`, `DrawTab`, ...). No ad-hoc draws.
- Buttons: `UiSkin::DrawToolItem` (labeled) + the interaction state machine `sky::ui::Button`
  (Normal/Hover/Pressed/Disabled). Do not hand-draw button hover/pressed.
- Editable values → `ReflectedFormView` (see `reflection-widget-framework.md`); edits go through
  `CommandService` (undoable). See AGENTS.md "编辑器配置/设置 UI".

### Rule for dialogs / viewers (asset viewer, preferences-like, etc.)

A dialog/viewer that edits or lists an asset MUST embed the **same** `ToolBar`/`MenuBar`
widgets (fed by actions) instead of drawing its own control row. A File menu with
Open/Close/Save and a toolbar with Save/Undo/Redo must look identical to the main window —
same icons, same spacing, same hover — because they are the same widgets + actions.
Register the dialog's commands as **scoped** actions (e.g. `scope = "assetViewer"`) and push the
scope (`EditorScopeStack`) while it is open; the chrome then resolves to that scope via
`ActionBarBuilder`. Do not build a private, local `MenuBar` with ad-hoc items.

## 3. Icons

- Assets: `engine/sandbox/resources/icons/<name>.svg` (plugin icons under a declared subdir,
  e.g. `resources/icons/aurora/`), **monochrome white**, 24x24 viewBox.
- Registration: the host scans icon roots for `*.svg` and registers each by file stem
  (`RasterizeSvg` -> `IUITextureRegistry::RegisterTexture` -> `EditorShell::SetIcon`, which also
  populates the cross-DLL `IconRegistry` name -> `UITextureId`). Roots come from
  `EditorExtension::GetIconDirectories()`: the default extension declares the built-in `icons`
  root, plugins declare their own (e.g. `icons/aurora`). No hard-coded name list.
- Draw: `UiSkin::DrawIcon` tints the texture with the theme color; `ToolBar`/`MenuBar` use it.
- Never ship text labels where an icon is the convention (toolbar = icon-only + hover tooltip).

## 4. Theme / sizing

- All colors from `UiColors`, all dimensions from `UiMetrics` on the process-wide `UiTheme`
  (`GetDefaultUiTheme()`, `MakeDarkTheme(scale)`). **No pixel literals** in widgets.
- See `docs/editor/ui-sizing-and-scaling.md`. Read it before any UI work.

## 5. Known gaps to close (unification backlog)

1. **Icon registry**: *done* — `editor/core/extension/IconRegistry` (cross-DLL name→`UITextureId`);
   `EditorShell::SetIcon` populates it; `UiSkin::DrawIcon` + `ToolItemButton::SetIcon`. Icons are
   loaded by scanning extension-declared roots (`EditorExtension::GetIconDirectories()`) instead of
   a hard-coded name list.
2. **Menu model**: *partly done* — `MenuBar::Item` supports `icon`, `shortcut`, `separator`.
   Remaining: `checkable/checked`.
3. **Action model**: *partly done* — `accelerator` (data-driven shortcuts) + `scope` landed;
   dialogs register scoped actions (`assetViewer.*`) and push `EditorScopeStack`. Remaining:
   replace magic `menuOrder` ints / group strings with shared enums.
4. **One chrome builder**: *done* — `ActionBarBuilder` builds the toolbar + menus (+ accelerator
   lookup) for the active scope; `EditorShell` and the asset viewer both use it.
5. **Extension seam**: *done* — plugin editor modules stage `EditorExtension`s through
   `EditorExtensionRegistry`; the host drains them (`EditorExtensionHost`) and reads their icon
   roots (`GetIconDirectories()`). Aurora/Content are live consumers.

Layout containers (`ui::StackPanel/DockPanel/GridPanel/Overlay`) now exist; migrate remaining
hand-computed rects (material-editor rows, preferences) onto them.

## 6. Quick checklist for a new editor view/dialog

- [ ] Commands registered as `EditorAction` (id/label/icon/group/menu/order/enabled/invoke).
- [ ] Toolbar via `widgets/ToolBar`; menu via `widgets/MenuBar` — not hand-drawn.
- [ ] Icon SVG under a scanned root (`resources/icons/` or an extension-declared root); name = file stem.
- [ ] Widgets painted via `UiSkin`; sizes/colors from `UiTheme` only.
- [ ] Editable values via `ReflectedFormView`; edits via `CommandService` (undoable).
- [ ] No hard-coded pixel rects for controls; use the shared layout/painters.
