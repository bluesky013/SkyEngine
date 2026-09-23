## Why

The non-Qt sandbox editor has a full toolkit-independent core (`EditorCore`: layout model, panel registry,
selection, property model, command/undo, document, console/log, extension host, viewport manager) and a UI
framework (`engine/ui`: `UIContext` element tree, widgets, `UIEventRouter`, style/theme), but **neither is wired
into the running editor**:

- The running editor's UI is a hand-drawn placeholder in `EditorRenderer` (immediate-mode `UIPaintContext` quads +
text) — no `sky::ui` element tree, no panels.
- `EditorCore` is referenced only by tests; the shell never instantiates or drives any core service.
- No input reaches the editor: `InputRouter` and `UIEventRouter` are unused in the run path, and there is no
  UI-wants-input gating.

This change wires the two together into a minimal, running shell so the editor can start being iterated as an
actual editor instead of a static mock.

## What Changes

- **Own and drive `EditorCore` from the editor host**: `SandboxModule` creates the core services, registers the
  default panels (`RegisterDefaultEditorPanels`), and builds the default layout.
- **Introduce an editor shell layer** (UI-linked, outside `EditorCore`) that turns the `editor-layout` model +
  `PanelRegistry` into a `sky::ui` `UIContext` element tree, and hosts the first panel views.
- **First panel set**: Outliner (selection), Inspector (property model), Console/Log (log + command services),
  and a Viewport container — each a `sky::ui` element subtree, not immediate-mode quads.
- **Wire input**: platform window events → `UIEventRouter` → focused panel; expose `UIContext::WantsInput()` for
  viewport input gating.
- **Render the shell**: `EditorRenderer` paints the `UIContext` draw data each frame (via the existing GUI
  pipeline) instead of drawing a hardcoded mock.
- **Register the built-in panels through `EditorExtensionHost`** so extensions remain the seam for adding panels.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
- `editor-ui-shell`: the shell is composed from the core panel registry + layout model into a `sky::ui` tree, and
  input is routed from the platform window through the UI event router with viewport gating.
- `editor-render`: the GUI pipeline consumes the shell's `sky::ui` draw data instead of a hand-drawn placeholder.

## Impact

- `engine/sandbox/module/**` (`SandboxModule` owns and drives `EditorCore`).
- New shell + panel sources under `engine/sandbox/` (UI-linked layer).
- `engine/sandbox/render/**` (`EditorRenderer` paints the `UIContext`; no more hardcoded quads).
- `engine/sandbox/app/**` (platform window events forwarded to the shell).
- Not changed: scene rendering inside the viewport (still placeholder; separate change), Metal (untested).
