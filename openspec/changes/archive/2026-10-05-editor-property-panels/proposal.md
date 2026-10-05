## Why

The reflection widget framework (`ReflectedFormView` + `ReflectedWidget` kinds) is built and proven in
the demo, but no real editor panel consumes it yet: `Inspector` still renders placeholder rows and there
is no global-config panel, even though `IEditorPropertySource` / `IEditorConfigSource` seams were
declared. Enum editing is also the last control still implemented inline in the view instead of as a
widget. This change finishes the widget split and wires the framework into the actual panels.

## What Changes

- Extract **Enum** into the widget system (`EnumReflectedWidget`), so `ReflectedFormView` keeps only
  form layout, scroll, default-reset and container (struct/sequence) behavior.
- Make the **Inspector** panel render the current selection through `IEditorPropertySource` +
  `ReflectedFormView` (no placeholder rows).
- Add a **global config** panel that renders named world configurations through `IEditorConfigSource` +
  `ReflectedFormView` (one section per configuration).
- Inject the default property source and the world-backed config source in the sandbox module, and
  register the panels in the default layout; add a non-const world-config accessor for write-back.

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities

- `editor-reflected-form`: enum members are edited by a widget like other kinds (view no longer
  special-cases enum).
- `editor-inspector`: the inspector renders via the reflected-form framework and a selection source.
- `editor-global-config`: the global config panel renders named configurations via the framework and a
  config source.

## Impact

- `engine/sandbox/shell`: new `EnumReflectedWidget`; `ReflectedFormView` removes its inline enum popup;
  `InspectorPanel` becomes a `ReflectedFormView` bound to a source; new global-config panel.
- `engine/sandbox/core`: `IEditorPropertySource` gains a small default `RegisteredPropertySource` usage;
  `IEditorConfigSource` unchanged.
- `engine/sandbox/module`: implements the selection/config sources, injects them, registers the config
  panel and default layout.
- `engine/framework`: additive non-const `World` config accessor for write-back.
- No new third-party dependencies.
