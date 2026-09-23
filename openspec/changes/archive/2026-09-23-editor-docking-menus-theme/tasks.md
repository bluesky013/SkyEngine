## 1. Theme

- [x] 1.1 Apply an editor `UITheme` (panel background/text, title, button colors) in the shell
- [x] 1.2 Style shell panels from the theme (style classes) instead of hardcoded colors

## 2. Tabs

- [x] 2.1 `TabHeader` renders a header row of panel titles for multi-panel tabs
- [x] 2.2 Clicking a header switches the active panel (updates the layout model's active index) and shows its body

## 3. Menu bar

- [x] 3.1 Engine-drawn tool/menu bar (`ToolBar`) with labeled action items
- [x] 3.2 View group toggles panel visibility through `LayoutModel` (hide / re-add), then rebuilds

## 4. Verify

- [x] 4.1 Build and run on Vulkan and DX12; themed shell renders, no errors
- [ ] 4.2 Confirm interactively: tabs switch, tool/menu items toggle panels (needs a manual run — layout changes are
      applied on the next frame via `pendingRebuild`)
