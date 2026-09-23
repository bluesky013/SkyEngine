## 1. Core ownership

- [x] 1.1 `SandboxModule` creates the `EditorCore` services (panel registry, layout model, selection, log service,
      command controller, extension host; `PropertyModel` is per-object and built on demand)
- [x] 1.2 Register the default panels (`RegisterDefaultEditorPanels`) and build the default layout

## 2. Shell layer

- [x] 2.1 Add the `EditorShell` target (`engine/sandbox/shell`), linking `EditorCore` + `UI` + `Core`/`Framework`
- [x] 2.2 Build a `sky::ui` `UIContext` tree from the layout model + panel registry; skip unregistered ids
- [x] 2.3 Provide a panel view-factory registry so panel ids resolve to element subtrees

## 3. Panels

- [x] 3.1 Outliner panel backed by `SelectionService`
- [x] 3.2 Inspector panel backed by `PropertyModel` (renders descriptors; empty state until an object is bound)
- [x] 3.3 Console/Log panel backed by `LogService` + `CommandController`
- [x] 3.4 Viewport container panel (placeholder titled frame for now)
- [x] 3.5 Register the built-in panels through `EditorExtensionHost` (`DefaultEditorExtension`)

## 4. Input

- [x] 4.1 Forward platform window pointer/key events to the shell and dispatch via `UIEventRouter`
- [x] 4.2 Expose `UIContext::WantsInput()` (set from hit-test/focus) and gate viewport input while UI is active

## 5. Render

- [x] 5.1 `EditorRenderer` paints the shell's `UIContext` draw data each frame; delete the hardcoded mock UI

## 6. Verify

- [x] 6.1 Build and run on Vulkan and DX12; the shell renders the default layout and panels (3 active tabs)
- [x] 6.2 Confirm layout/panel changes are reflected without touching `EditorRender` (draw data comes from the shell)
- [ ] 6.3 Confirm input routes to panels (hover/click/focus) and the viewport is gated while UI is active
