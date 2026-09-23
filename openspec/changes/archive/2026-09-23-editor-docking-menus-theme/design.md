## Context

The shell (`EditorShell`) builds a `sky::ui` element tree from the `editor-layout` model + `PanelRegistry` and
paints it. Today: panels are flat titled frames with hardcoded colors; a `Tab` node renders only its active panel;
there is no menu bar and no interaction beyond the layout model.

`sky::ui` provides what is needed: `UITheme`/`UIStyle` (class-based styles resolved per element), style classes on
`UIElement`, `UIElement::OnPointerEvent`/`OnPointerEnter/Leave`, and a `UIEventRouter` already wired to the host.

## Goals / Non-Goals

**Goals:**
- Theme the shell through `UITheme` (no hardcoded panel colors).
- Make multi-panel tabs usable: header row + click to switch the active panel.
- Provide an engine-drawn menu bar with drop-down menus; View toggles panel visibility.

**Non-Goals:**
- Drag-to-re-dock (dragging a tab into another area) and splitter drag-resize — follow-up.
- External theme files / a theme editor.
- New panel content.

## Decisions

- **D1 - Theme via `UITheme` style classes.** The shell sets styles on `UIContext::GetTheme()` (e.g. `panel`,
  `panel-title`, `menu-bar`, `menu-item`); panels attach classes and resolve colors from the theme. Alternative:
  keep hardcoded colors — rejected, it blocks theming and duplicates constants.
- **D2 - `TabView` element owns header + active body.** A tab node becomes one `TabView` that draws a header row
  (one title per panel) and hosts only the active panel's view. Clicking a header sets the tab's `activeIndex` and
  asks the model/shell to rebuild. Rationale: keeps interaction local and the model authoritative.
- **D3 - `MenuBar` element with drop-downs; actions via callbacks.** The menu bar draws titles and, when open, a
  drop-down list; selecting an item invokes a callback (`EditorShell` exposes `SetMenuAction`/visibility toggles).
  Rationale: no engine Menu widget exists, and a callback keeps the shell free of application concerns.
- **D4 - Visibility is model-driven.** View toggles call `LayoutModel::ClosePanel` (hide) and re-add via
  `SplitPanel` (show), then rebuild; the shell never tracks hidden panels itself.

## Risks / Trade-offs

- [Rebuilding the tree on each tab click/menu action] → acceptable for editor UI; elements are cheap; can optimize
  with incremental updates later.
- [Overlapping concerns between TabView and layout ratios] → TabView only handles the tab header; split ratios stay
  in the layout model.
- [Drop-down hit-testing overlaps panels] → the menu bar is drawn last (higher z) and consumes pointer events while
  open.

## Migration Plan

1. Apply the theme + panel style classes (visual only).
2. Introduce `TabView` (header + active body + click switch).
3. Add `MenuBar` with View (toggle panels) and Help items.
4. Verify on Vulkan/DX12; confirm no errors and tabs/menus render.
