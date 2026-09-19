## Why

The archived `engine-ui-system` change specified a data-driven UI layer but shipped only the core foundation and
three widgets. `openspec/specs/ui-data` requires a document format, loader, element registry, binding model, and
a C++ builder. None of this depends on the GPU, so it can land while the Aurora render pass waits. Delivering it
now makes screens authorable as data and lets later widgets/text plug into the same registry and binding path.

## What Changes

- Add a `UIElementRegistry` mapping document type names to element factories (seeded with `Panel`, `Image`, `Button`).
- Add a JSON UI document loader producing a `UIElement` tree, with name lookup and load diagnostics (never throws).
- Add a property model so the loader can apply common, layout, and type-specific properties through a virtual
  `SetProperty` path on elements.
- Add `UIDataContext` + `IUIDataProvider` + `UIBinding` with change notification and built-in converters
  (`remap`, `format`, `boolToVisible`).
- Add a C++ `UIBuilder` that produces the same tree and bindings as the loader (parity).
- Resolve document texture refs through `IUIAssetResolver`.
- Tests for parse, registry, unknown-type/missing-asset diagnostics, theme swap, binding update, and builder parity.

**Non-goals**: the `Text` widget and text layout, event/event-graph bindings, a visual editor, document-to-code
generation, hot reload, and binary document caching.

## Capabilities

### New Capabilities
<!-- None: this implements and refines the existing `ui-data` capability. -->

### Modified Capabilities
- `ui-data`: add concrete requirements for built-in converters, binding evaluation timing, and load diagnostics.

## Impact

- New `engine/ui/core/include/ui/data/` and `engine/ui/core/src/data/` sources; `UIElement` gains a virtual
  `SetProperty`; new `UIPropertyValue`.
- `UI` links `3rdParty::rapidjson` (already used by `Core`).
- New tests in `engine/test/ui/` (data suite). No `UIRender` or Aurora changes.
