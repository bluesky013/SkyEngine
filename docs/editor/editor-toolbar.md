---
title: "Editor Toolbar"
description: "Design for a quick-action toolbar (Open/Save/Undo/Redo) in the Sandbox editor, with a UE/JetBrains/VS Code/Unity comparison."
updated: "2026-10-07"
---

## Goal

A **quick-action toolbar** in the Sandbox editor: first cut = **Open, Save, Undo, Redo** (handlers already
exist for open/save; undo/redo is in `EditorCore::GetCommandService`). Keep it small and consistent with the
existing engine-drawn chrome; extend later with tool modes / play controls.

## Current state (facts)

- Chrome: **menu bar** at the top (`headerHeight` = 24), **status bar** at the bottom (`footerHeight` = 22),
  the **dock area** between. No toolbar.
- Theme already reserves `UiColors::toolbar` and `UiMetrics::toolbarHeight` (+ `iconButtonWidth`,
  `controlButtonWidth`); `UiSkin::DrawToolItem(rect, label, hovered)` draws a text-label button (used by the
  menu bar and dialog buttons). **No UI-side icon draw helper** yet.
- Icons: an SVG→RGBA pipeline exists (`UiIconBuilder` via nanosvg + DDC) and the UI renderer supports
  textures; but there is no `PaintContext` "draw icon" call, so the toolbar starts **text-first**.
- Undo/redo: `EditorCore::GetCommandService()` → `Undo() / Redo() / CanUndo() / CanRedo()`. The `Edit > Undo`
  menu item is currently a **no-op**.
- Handlers: `EditorShell` already exposes `SetOpenWorldHandler`, `SetSaveWorldHandler` (wired by the module).

## Comparison

| Editor | Toolbar model | Placement | Notes |
|---|---|---|---|
| **Unreal** | Full-width row under the menu; **large icons + labels**; grouped (File/Content/Play/Modes/Tools) | Top, left→right | Prominent; many actions; icon-first with tooltips |
| **JetBrains** (IntelliJ) | Slim **main toolbar**: small **icon** actions (Open, Save All…) + a centered **Run config** widget; tool windows as side bars; a separate nav bar | Top, compact | Minimal, icon-first, tooltips; actions also in menus |
| **VS Code** | No classic toolbar; a **title bar** with a center command/search box + menus; everything via the **Command Palette** | Top center | Command-palette-first, not button-first |
| **Unity** | Toolbar with **transform tool modes** (Q/W/E/R), **Play / Pause / Step** (left), Layers/Layout (right) | Top | Tool-mode-centric + play controls |
| **Blender** | Top bar = editor-type dropdown + menus; a left **tool shelf** for tools | Top / left | Mode + tool shelf |

**Takeaways**
- A slim **top toolbar** is the common shape (UE, JetBrains, Unity). UE = icon+label and busy; JetBrains =
  compact icons; VS Code avoids buttons (palette) — not our model (we already have a menu bar).
- **Icons matter** for compactness (UE/JetBrains), but we lack a UI icon draw path today.
- Play/transform controls (Unity/UE) are a *different* group than file/undo — keep them out of this first cut.

## Recommendation

Adopt a **slim left-aligned top toolbar**, directly under the menu bar, **text-first** (consistent with the
current chrome), with **icons as a follow-up**. Items for v1:

```
[ Open ] [ Save ]   |   [ Undo ] [ Redo ]
```

- **Placement**: top row under the menu; height `metrics.toolbarHeight`; color `colors.toolbar`.
- **Behavior**: click runs the action; **disabled** (dimmed) when unavailable (Undo/Redo via
  `CanUndo/CanRedo`); hover highlight; tooltips ("Open World", "Save (Ctrl+S)", "Undo (Ctrl+Z)", …).
- **No mode/play/tool groups** in v1 (leave room to append later).
- Keep the **menu bar** as the complete action list; the toolbar is a shortcut strip (JetBrains/UE hybrid).
- **Icons later**: once a UI icon draw path exists, switch each item to `icon + optional label`, sourcing
  SVGs through the existing `UiIconBuilder`/DDC and registering a small UI icon set.

## Design

### Component
- New `editor/shell/widgets/ToolBar.h/.cpp` (mirrors `MenuBar`/`StatusBar`):
  - `struct Item { std::string label; std::string tooltip; std::function<void()> action; bool enabled; }`.
  - `SetItems(...)`; `SetUndoRedo(bool canUndo, bool canRedo)`; hit-testing + hover; `OnPaint`/`OnPointerEvent`.
  - Paints with `UiSkin::DrawToolItem` (dim when `!enabled`); separators as a thin `uc::VLine`.

### Shell integration
- `EditorShell`: own `toolBarElement`; create it in `Rebuild()` (`context->AddChild`), next to the menu/status
  bars; wire item actions to `openWorldHandler` / `saveWorldHandler` / new `undoHandler` / `redoHandler`.
- **Layout shift**: the dock area, splitters, and `ApplyNode` rect top change from `headerHeight` to
  `headerHeight + toolbarHeight` (three call sites: `Rebuild`, `CreateSplitters`, `Layout`/`UpdateSplitters`).
  Toolbar bounds = `{0, headerHeight, width, headerHeight + toolbarHeight}`.
- New handlers on `EditorShell`: `SetUndoHandler`, `SetRedoHandler`, and `SetUndoRedoState(bool, bool)` (the
  module pushes state; the shell also refreshes the `Edit` menu items' enabled state).
- Module wiring: `undo/redo` → `EditorCore::GetCommandService().Undo()/Redo()`; push
  `CanUndo/CanRedo` once per tick (cheap; same place as `RefreshDocumentInfo`).
- Menu parity: wire the existing `Edit > Undo` (and add `Redo`) to the same handlers; add `Ctrl+Z` / `Ctrl+Y`
  (and `Ctrl+Shift+Z`) to `EditorShell::DispatchKey`.

### Sizing / scaling
Everything from the theme per `ui-sizing-and-scaling.md`: `metrics.toolbarHeight`, `metricses.itemSpacing`,
`padX`, `controlPad`, `iconButtonWidth`, `fonts.label`. No layout literals.

## Non-goals (this cut)

- UI SVG icons in the toolbar (needs a `PaintContext` texture/icon draw + an icon registry).
- Transform-tool modes, Play/Pause/Step (`F5` already exists), mode switchers.
- User-customizable / hideable toolbar, overflow menu.

## Testing

- `EditorShellTest`: toolbar item hit-testing + click → handler called once; disabled items don't fire;
  `SetUndoRedoState` toggles enabled; the dock area's top equals `headerHeight + toolbarHeight` after layout.

## Status (implemented)

- `editor/shell/widgets/ToolBar` hosted by `EditorShell` between the menu bar and the dock area.
- **Extension point**: `editor/core/extension/EditorActionRegistry` — a toolkit-agnostic, cross-DLL
  `Singleton` of `EditorAction{id,label,icon,group,order,enabled,invoke}`. Plugins contribute through their
  `EditorExtension::Register()`. The shell builds the toolbar from it (grouped + separators) and refreshes
  enabled state each tick (`EditorShell::RefreshActions`).
- **Built-in actions** (registered by the module): Open / Save (file), Undo / Redo (history, enabled from
  `CommandService::CanUndo/CanRedo`), Play / Pause / Stop (play, enabled from `PlaySession` state).
- **Icons**: SVG icons (`engine/sandbox/resources/icons/*.svg`) are rasterized (nanosvg) into RGBA and
  registered as UI textures (`IUITextureRegistry`); `EditorShell::SetIcon` maps name → texture and the toolbar
  draws them via `AddTexturedQuad`. Items are **icon-only** (label reserved for a hover tooltip); items without
  an icon fall back to text.
- Menu parity: `Edit` menu `Undo`/`Redo`; `Ctrl+Z` / `Ctrl+Y` (and `Ctrl+Shift+Z`) shortcuts.

## Follow-ups

- Hover tooltips (show the label), icon dim/tint polish.
- Tool modes / Play as a combined control, user-customizable / hideable toolbar.
