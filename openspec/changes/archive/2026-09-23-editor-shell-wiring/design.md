## Context

Three layers exist but are not connected:

- `EditorCore` (`engine/sandbox/core`, `Core`+`Framework` only, guarded against UI/Aurora/Qt): `PanelRegistry`,
  `LayoutModel`/`LayoutPersistence`, `SelectionService`, `PropertyModel`, `CommandService`, `DocumentService`,
  `LogService` + `CommandController`, `EditorExtensionHost`, `ViewportManager`, `InputRouter`. Referenced only by
  tests today.
- `engine/ui` (`UI`/`UIRender`): `UIContext` element tree (`UIElement`, layout, paint, focus, animations, theme,
  `WantsInput`), `UIEventRouter` (hit-test/focus/capture/hover), widgets (Button/EditBox/ListView/Panel/
  ScrollView/HBox/VBox/Text/Image), `UIRenderer` (consumes `UIDrawData`).
- `EditorRenderer` (`engine/sandbox/render`): Aurora device + GUI pass; currently paints a hardcoded
  immediate-mode mock (rects + text) via `UIPaintContext`.

The goal is a minimal but real shell: core drives *what* panels exist and *where*, `sky::ui` renders them,
input reaches them, and `EditorRenderer` just draws the resulting draw data.

## Goals / Non-Goals

**Goals:**
- Own and drive `EditorCore` from the editor host (`SandboxModule`).
- Build the shell as a `sky::ui` `UIContext` tree from the `editor-layout` model + `PanelRegistry`.
- Provide the first panels (Outliner, Inspector, Console/Log, Viewport container).
- Route platform input through `UIEventRouter` and gate viewport input via `WantsInput`.
- Replace `EditorRenderer`'s hardcoded mock with the shell's draw data.

**Non-Goals:**
- Scene rendering inside the viewport (still a placeholder image).
- Docking/tab drag interaction and layout editing (model exists; interaction is a later change).
- Menus/toolbars and theming polish.
- Metal validation; a full menu/property editors; running real editor extension modules.

## Decisions

- **D1 - The shell lives in a new UI-linked sandbox layer, not `EditorCore`.** Add `engine/sandbox/shell`
  (target `EditorShell`, links `EditorCore` + `UI` + `Core`/`Framework`). Rationale: `EditorCore` is guarded to be
  UI-free; panels are views, so they must live above it. Alternative considered: put panels in `EditorRender`.
  Rejected — `EditorRender` is the RHI host; keeping UI composition out of it keeps the RHI-linked surface small.
- **D2 - `SandboxModule` owns the `EditorCore` instance.** The host creates the services, registers default panels
  (`RegisterDefaultEditorPanels`), and loads the default layout; it passes the core to the shell and the renderer.
  Rationale: one owner, mirrors the module-as-editor-host design.
- **D3 - Panels are `sky::ui` element subtrees produced by panel views.** Each panel id resolves to a view factory
  that builds its element tree from the relevant core service (Outliner←`SelectionService`, Inspector←`PropertyModel`,
  Console/Log←`LogService`+`CommandController`, Viewport←placeholder image element). Built-in panels are registered
  through `EditorExtensionHost` so extensions remain the seam for adding panels.
- **D4 - Input flows platform → shell → `UIEventRouter`.** The host forwards window pointer/key events to the
  shell, which dispatches through `UIEventRouter`; `UIContext::WantsInput()` is propagated so viewport input is
  gated while UI is active.
- **D5 - `EditorRenderer` paints the shell.** Each frame: `UIContext::SetContentSize` (window size) → `Layout()` →
  `Paint(paintContext)` → `UIRenderer.UpdateDrawData/EnsureTextureReady/Render`. The hardcoded mock is deleted;
  the GUI pass and viewport placeholder stay.
- **D6 - Scope the first shell to a static default layout.** Splits/tabs are rendered from the model but not
  interactively edited yet; that keeps this change small and reviewable.

## Risks / Trade-offs

- [`EditorCore` services are currently test-only; wiring may reveal missing APIs] → start with the services the
  first panels need (selection/property/log/command) and extend core minimally where required.
- [Panel→service coupling can grow in the shell layer] → panel views talk only to core interfaces; the renderer
  never touches core.
- [Placeholder viewport may be mistaken for real scene rendering] → keep it clearly labelled; scene integration is
  a separate change.
- [Two UI sources during transition] → delete the mock in the same change so there is a single source of truth.

## Migration Plan

1. Add `EditorShell` target + `EditorCore` ownership in `SandboxModule`; build an empty shell (root + panels from
   the default layout) and verify it draws.
2. Add the first panel views; wire selection/property/log/command reads.
3. Replace `EditorRenderer`'s mock with shell draw data.
4. Forward platform input and gate the viewport.
5. Rollback: restore the mock renderer and drop the shell target; `EditorCore` is unchanged.
