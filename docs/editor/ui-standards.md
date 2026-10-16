# Editor UI Standards (Sandbox Framework)

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
a.group = EditorGroup::File;  // toolbar group (None = not on the toolbar)
a.menu = EditorMenu::File;
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

**Global menu bar vs context menu**: the menu bar (`menu != EditorMenu::None`) holds only
operations that are independent of the current selection (e.g. Assets: `Refresh`, `New Asset`,
`Import…`, `Cook All Sources`). Selection-scoped operations (Open / Cook / Rename / Move /
Duplicate / Delete / Show in Explorer / …) leave `menu` unset so they are **not** on the menu bar;
register them by id and invoke them from the owning panel's **context menu** (see
`AssetBrowserPanel`). A menu entry that is greyed out until something is selected is a smell.

## 2. Use the shared widgets (mandatory)

Reusable controls live in `engine/sandbox/widgets` as **`EditorWidgets`**, with flat
`editor/widgets/<Name>.h` includes. Editor feature modules link this target without linking
`EditorShell`. `EditorWidgets` depends on `EditorUi`, `EditorCore`, `UI`, `Core`, and the required
Framework reflection interfaces; it does not depend on Shell or Aurora. Shell keeps window,
docking, modal-dialog, and concrete panel/viewer composition.

- Toolbar → `editor/widgets/ToolBar.h` (items from `ActionBarBuilder`, icon-only + tooltip).
- Menu bar → `editor/widgets/MenuBar.h` (menus from `ActionBarBuilder`; two levels max today).
- Panels/dialogs → `UiSkin` painters (`DrawPanel`, `DrawSectionHeader`, `DrawRow`, `DrawField`,
  `DrawToolItem`, `DrawCheckbox`, `DrawSlider`, `DrawPopup`, `DrawTab`, ...). No ad-hoc draws.
- Buttons: `UiSkin::DrawToolItem` (labeled) + the interaction state machine `sky::ui::Button`
  (Normal/Hover/Pressed/Disabled). Do not hand-draw button hover/pressed.
- Editable values → `ReflectedFormView` (see `reflection-widget-framework.md`); edits go through
  `CommandService` (undoable). See AGENTS.md "编辑器配置/设置 UI".

Each form owns its interaction widgets; extend that form through `view.Widgets().Register(...)`.
The default form constructor uses the process-shared editor services. Pass `ReflectedFormContext`
to select a document's command history, property metadata, change notifier, and theme; those
referenced objects must outlive the form. Popup, drag, and text-edit state remain local to the form.

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

- All colors from `UiColors`, all dimensions from `UiMetrics` on the active `UiTheme`.
  `GetDefaultUiTheme()` is shared across DLLs; an explicit `ReflectedFormContext` can supply a
  host-owned theme instead. Use `MakeDarkTheme(scale)` for scaling. **No pixel literals** in widgets.
- See `docs/editor/ui-sizing-and-scaling.md`. Read it before any UI work.

## 5. Current status and follow-ups

Shared IconRegistry, UiSkin::DrawIcon, extension icon-root scanning, ActionBarBuilder, scoped
actions/accelerators, and EditorMenu/EditorGroup enums are implemented. MenuBar::Item supports
icon, shortcut, separator, and checkable/checked display. EditorAction now supplies enabled,
visible, checkable and checked state through registration snapshots and guarded registry invocation.
Shell/Viewer refresh menu and toolbar state in place; hidden entries have no paint/hit slots or
accelerator, while disabled accelerators are consumed without invocation. Retained controls cannot
invoke replaced registrations. Per-owner scope identity and plugin unload cleanup remain **T10-B**
in [the reviewed backlog](ui-layout-and-chrome-plan.md#9-reviewed-backlog-2026-10-16).

Layout containers (`ui::StackPanel/DockPanel/GridPanel/Overlay`) exist; the asset-viewer top
chrome and material-editor rows use them. PreferencesDialog still uses hand-computed rects
(**UI-11**). Paired font-registry detach, explicit cancel/abandon binding semantics, asset-session
history/persistence, and service-wide preview cancellation now have dedicated repairs and regressions
(**UI-01/UI-03–UI-07**). Shared texture replay and deferred surface/resource release are implemented;
their real floating-window GPU/visual gates remain open (**UI-02/UI-08**). See the task board for
executed checks and remaining Win32/native acceptance.

## 6. Quick checklist for a new editor view/dialog

- [ ] Commands registered as `EditorAction` (id/label/icon/group/menu/order/enabled/invoke).
- [ ] Link `EditorWidgets`; toolbar/menu via `editor/widgets/ToolBar.h` / `MenuBar.h`.
- [ ] Icon SVG under a scanned root (`resources/icons/` or an extension-declared root); name = file stem.
- [ ] Widgets painted via `UiSkin`; sizes/colors from `UiTheme` only.
- [ ] Editable values via `ReflectedFormView`; edits via `CommandService` (undoable).
- [ ] No hard-coded pixel rects for controls; use the shared layout/painters.
