## Why

The editor shell is wired but static and unstyled: the layout is fixed (multi-panel tabs show only their active
panel, with no way to switch), panels hardcode colors, and there is no engine-drawn menu bar or toolbar. The
`editor-ui-shell` intent (panels on the in-house UI framework, engine-drawn menus and theming) and the
`editor-layout` intent (docking/interaction provided by the layout model) are not yet exercised in the running
editor.

## What Changes

- **Theme**: the shell applies an editor `sky::ui::UITheme` (panel background/text, title, button colors) and
  panels style themselves from the theme instead of hardcoded constants.
- **Tab headers + switching**: a tab with multiple panels renders a header row of titles; clicking a title switches
  the active panel (so the Inspector and Console become reachable). The active panel's view is shown.
- **Engine-drawn menu bar**: a top menu bar with drop-down menus (e.g. View, Help); selecting an item runs an
  editor action. The **View** menu toggles panel visibility through the layout model.
- **Layout-driven visibility**: showing/hiding a panel goes through `LayoutModel` (`ClosePanel` to hide, re-add via
  `SplitPanel` to show), keeping the shell a pure view of the model.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
- `editor-ui-shell`: engine-drawn menu bar, theme-driven panel styling, and tab headers that switch the active
  panel.
- `editor-layout`: panel visibility toggling is expressed through the layout model.

## Impact

- `engine/sandbox/shell/**` (`EditorShell`: theme, `TabView`, `MenuBar`, visibility toggles).
- `engine/sandbox/module/src/SandboxModule.cpp` (wire menu actions / initial theme).
- Not changed: the editor module services, the renderer, and the RHI.
