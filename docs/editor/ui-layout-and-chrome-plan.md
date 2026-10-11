# Editor UI — Layout & Chrome Plan

Goal: converge the editor UI on one architecture so future work is **incremental**, not a
big-bang refactor. Companion to `docs/editor/ui-standards.md` (the rules) and
`ui-sizing-and-scaling.md` (theme/sizing), `editor-toolbar.md`, `editor-framework-design.md`.

## 1. Why (current pain)

- `sky::ui::UIElement` uses **explicit absolute bounds**; there is a `Measure()` (preferred-size)
  pass but **no parent-driven Arrange pass**. So every owner of children hand-computes
  `*Rect()` each paint (`PanelRect/RightRect/CloseRect/ToolbarRect/...`, material-editor row
  rects). Adding one control forces editing a pile of rect math.
- Chrome (toolbars/menus) is **not unified**: the main window drives `widgets/ToolBar` +
  `widgets/MenuBar` from `EditorActionRegistry`, but dialogs/viewers (asset viewer) hand-draw
  their own control rows → "east one, west one".
- Icons/menus/accelerators have no single registry/seam.

## 2. Target architecture (layers, bottom-up)

```
Theme        UiTheme (UiColors/UiMetrics/UiFonts) — single source of dims+colors
Layout       Layout containers: StackPanel(H/V), Grid, Dock, Overlay + slot model
Widgets      Button, Field, Label, Checkbox, Slider, Tabs, ... painted by UiSkin
Icons        IconRegistry (name -> UITextureId) + UiSkin/UiDraw icon draw helper
Chrome       EditorActionRegistry (single source) -> ToolBar / MenuBar / shortcuts
             + ActionBarBuilder (EditorAction -> ToolBar::Item / MenuBar::Item)
Views        Editor views/dialogs = containers + widgets + chrome + ReflectedFormView
             Edits flow through CommandService (undoable)
```

### 2.1 Layout model (the key missing piece)

Two-pass, WPF/Slate/Flutter-style, added to the `ui` core:

- `UIElement::Measure(const LayoutConstraint&, outSize)` (bottom-up preferred size; exists ~).
- `UIElement::Arrange(const UIRect&)` (top-down final rect; **new**).
- Container elements implement both and position children; leaves just report size.
- **Slot model** (per child): `Width/Height = Auto | Fill(weight) | Px`, `HAlign/VAlign`,
  `Margin`. Parents read slots; children never know pixels.

Containers (start minimal):
- `StackPanel` (orientation H/V, gap, padding, Auto/Fill) — covers toolbars, form rows, button
  groups (80% of cases). **Do this first.**
- `Grid`, `DockPanel`, `Overlay` — later, as needed.

Retained tree stays; this adds the missing layout pass. No change to `sky::ui` rendering.

### 2.2 Chrome model (already mostly there)

`EditorActionRegistry` (cross-DLL Singleton) is the **single declarative source**: one
`EditorAction{id,label,icon,group,menu,submenu,order,enabled,invoke}` drives toolbar + menu +
shortcut by **id**. Keep it. Close the gaps:
- `ActionBarBuilder`: one place mapping `EditorAction` → `ToolBar::Item` + `MenuBar::Item`
  (shared ordering/tooltip/enabled/icon resolution).
- Menu model: add `icon`, `shortcut`, `separator`, `checkable/checked` to `MenuBar::Item`.
- `accelerator` field on `EditorAction` → data-driven shortcuts (replace the `DispatchKey`
  switch).
- Presentation enums/constants instead of magic `menuOrder` ints / group strings.
- Extension seam: plugin `EditorExtension::Register()` actions + plugin icon roots.

### 2.3 Icons

- `IconRegistry` (name → `UITextureId`), populated by **scanning** `resources/icons/*.svg`
  (+ plugin roots) instead of the hard-coded `kIcons[]`; keep the white-SVG → RGBA → tint path.
- Add `UiSkin`/`UiDraw` icon helper (today only `ToolBar` draws icons).

## 3. Guiding principles (what prevents future refactors)

1. **Declare intent, not pixels.** Views describe layout via containers + slots; no manual rects.
2. **Containers own child placement.** A view never sets a child's bounds except as the root of a
   container subtree.
3. **One source per concern**: actions (registry), icons (IconRegistry), theme (UiTheme),
   editable data (reflection), edits (CommandService).
4. **Dialogs/viewers reuse the same chrome** (`ToolBar`/`MenuBar` + actions), not bespoke rows.
5. **Paint via UiSkin; sizes/colors via UiTheme.** No hard-coded pixels, no ad-hoc draws.
6. **Separate edit from commit** (buffer + Save), and never run heavy work inside event dispatch.

## 4. Phased plan (each phase shippable; no big-bang)

**P0 — freeze the rules.** `docs/editor/ui-standards.md` + AGENTS rule. *(done)*

**P1 — `StackPanel` + slot model.** *(done)* `ui::StackPanel` (H/V, Auto/Fixed/Fill + weight +
margin, gap/padding; Measure + Arrange + `RoutePointer`); asset-viewer toolbar migrated to it.

**P2 — icons.** *(done)* `editor/core/extension/IconRegistry` (cross-DLL name→`UITextureId`);
`EditorShell::SetIcon` populates it; `UiSkin::DrawIcon`; `ToolItemButton::SetIcon` (icon-only +
hover). Asset-viewer toolbar shows `save/undo/redo` icons.

**P3 — chrome in dialogs/viewers.** *(done, 方案 1 / dialog-scoped)* Asset viewer embeds
`widgets/MenuBar` + `widgets/ToolBar`(StackPanel+ToolItemButton); File(Open/Save/Close) +
Edit(Undo/Redo) local actions; menu items show shortcut hints; accelerators wired in
`OnKeyEvent` (Ctrl+S/O/Z/Y, Esc).

**P4 — chrome model enrichment.** *(`MenuBar::Item` has `shortcut` done; remaining)*
`ActionBarBuilder`; menu item **icon** + **separator** + checkable; replace magic `menuOrder`
/ group strings with enums; data-driven accelerators.

**P5 — more containers.** `Grid`, `DockPanel`, `Overlay`; migrate remaining hand rects
(preferences dialog, other panels, material-editor row layout).

**P6 — extension seam.** Route plugin actions + icon roots through the registry.

**P7 — global scoped actions (方案 2).** Give `EditorActionRegistry` a scope/context so dialog
actions (asset viewer) resolve to the active window's handler (VS Code `when`-style) instead of
per-dialog local menus.

Each phase: build + run `EditorShellTest` + the asset-viewer manual checks; keep old behavior
until the new path is verified, then delete the old code in the same commit.

### Done so far vs plan
- **P0** done (rules).
- **P1** done: `ui::StackPanel` (H/V, Auto/Fixed/Fill) + slot model; asset-viewer toolbar built from it.
- **P2** done: `IconRegistry` + `UiSkin::DrawIcon` + `ToolItemButton` (icon-only).
- **P3** done (方案 1): asset viewer embeds `MenuBar` + `StackPanel`/`ToolItemButton`; File(Open/Save/Close) + Edit(Undo/Redo), accelerators (Ctrl+S/O/Z/Y, Esc).
- **P4** mostly done: `MenuBar::Item` now supports `shortcut`, `icon`, `separator`; accelerators wired. Remaining: `ActionBarBuilder`, enums for groups/order.
- **P5** mostly done: containers `StackPanel`/`DockPanel`/`GridPanel`/`Overlay`; **asset-viewer top chrome migrated to `DockPanel`** (menu/toolbar/body, no hand offsets). Remaining: migrate the material-editor row layout off `ComputeRowRects`.
- **P6** partly done: `EditorExtensionRegistry` (cross-DLL `Singleton`) lets plugin editor modules
  stage `EditorExtension`s in `Init`; `SandboxModule` drains it into the host before `RegisterAll`.
  Remaining: a plugin consumer (none yet) + plugin icon roots into `IconRegistry`.
- **P7** pending (design): scoped/context actions in `EditorActionRegistry`.
- Open is now the real action (open the asset source with the OS); non-Windows is a no-op.
- Known build-infra flake (not this work): `Aurora.ShaderCompiler.Static`'s `ShaderHeaderTool` custom command needs its CWD/PATH to find the slang DLLs.

## 5. Migration compatibility

- Introduce layout containers **additively** (new `UIElement` subclasses); existing absolute-bounds
  usage keeps working during migration.
- Migrate one screen at a time; a screen is "converted" when it has no manual `*Rect()` for
  controls.
- Keep `UiTheme`/`UiSkin` as the only style source throughout — no parallel styling.

## 6. Code placement

- Layout + widgets: `engine/ui/core` (`ui/widgets/*`, layout containers).
- Theme/skin/painters: `engine/sandbox/shell` (`UiTheme`, `UiSkin`, `UiDraw`).
- Chrome widgets: `engine/sandbox/shell/widgets` (`ToolBar`, `MenuBar`, future `ActionBarBuilder`).
- Actions/icons: `engine/sandbox/core` (`EditorActionRegistry`), icons in `resources/icons`.
- Views: shell panels/dialogs; asset views via `IAssetViewProvider` (owning plugin editor
  modules) using the shared chrome.

## 7. Open decisions

- Layout constraint type (`LayoutConstraint` = available size + measure flags) vs simpler
  `Measure(availableW, availableH)` + auto height — pick the minimal that still supports Fill.
- Whether `StackPanel` lives in `ui` core (reusable by any UI host) or shell — recommend `ui`.
- Menu depth: keep 2 levels or generalize to N (affects `MenuBar`).

## 8. P7 design — scoped / context actions

**Problem.** The global `EditorActionRegistry` drives chrome by id, but actions are *context
dependent* (the asset viewer's Save/Undo act on the open asset). Today dialogs build a **local**
`MenuBar` with local actions (方案 1) — works and reuses the widgets, but every dialog repeats it and
ids can silently collide with the main window.

**Target (VS Code-style):** one global command set + a **scope/context** so the active window
resolves the handler.

- `EditorAction` gains (all additive; defaults keep today's global behaviour):
  - `scope` (string; default `"global"`) — e.g. `"assetViewer"`.
  - `accelerator` (string, data-driven) — replaces the `EditorShell::DispatchKey` switch.
  - `when` (optional context predicate, e.g. `assetDirty`) for enable/visible.
- **Scope stack** (`EditorScopeStack`, cross-DLL): the shell pushes the active window/dialog scope
  on open, pops on close; the top is "active".
- `ToolBar`/`MenuBar` + a new `ActionBarBuilder` render actions whose `scope` is the active scope
  (or `"global"`), in a shared order. Dialogs **stop building local menus** and instead register
  scoped actions; the same chrome widgets render them.
- Handler resolution: same id in different scopes → the active scope wins.

**Migration (incremental, each step shippable):**
1. Add `scope` + `accelerator` to `EditorAction` (existing actions = `"global"`).
2. `ActionBarBuilder` (`EditorAction` → `ToolBar::Item`/`MenuBar::Item`, filtered by active scope);
   have `EditorShell` and the asset viewer both use it.
3. Asset viewer: register `asset.open/save/close` + `edit.undo/redo` (scope `"assetViewer"`);
   delete the ctor's local `MenuBar` and the `AssetViewerWidget::OnKeyEvent` key switch (consumed
   via the active scope's accelerators).
4. Also route plugin `EditorExtension::Register()` actions through the same registry (ties into P6).

**Risks:** scope lifetime with nested dialogs; id collisions; `when` predicates. Start with plain
scope filtering (no `when`) and add predicates only where needed.

## 9. Current backlog (handoff)

**Done:** P0–P5 (layout containers + asset-viewer chrome migrated to `DockPanel` + icons +
accelerators), P6 foundation (`EditorExtensionRegistry`), P4 menu `shortcut`/`icon`/`separator`.

**Next, in rough priority:**

1. **P7 step 1** (additive): `EditorAction` gains `scope` + `accelerator`; add `ActionBarBuilder`.
   Then move the asset viewer to registered scoped actions and delete its local `MenuBar` +
   `OnKeyEvent` key switch.
2. **P6 remainder**: a plugin consumer of `EditorExtensionRegistry` + plugin icon roots
   (`IconRegistry` directory scan instead of the hard-coded `kIcons[]`).
3. **P5 remainder**: migrate the material-editor row layout off `ComputeRowRects` onto containers.
4. Menu `checkable`; shared enums for action group/order (drop magic strings/ints).
5. Material editor `defaults` + per-property **reset** (format TBD: sibling `defaults` map vs inline
   `{value,default}`); also add/delete for material-instance overrides.
6. Remove `assets/techniques/*.tech` + their `configs/render_preload_assets.json` entries once the
   render side no longer needs them (paired with the shader migration; see
   `docs/aurora/shader-design-and-migration.md`).
7. **openspec `editor-asset-views`**: all tasks checked; the DATA **source write-back** (改源重 cook)
   is the deferred MVP item; archive when ready.

**Known build-infra issue (not this work):** `Aurora.ShaderCompiler.Static`'s `ShaderHeaderTool`
custom command needs its CWD/PATH to find the slang runtime DLLs, else it exits non-zero and fails
the build. Reportedly fixed on another machine — verify locally after a clean reconfigure.


