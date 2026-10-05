## Context

The reflection widget framework is in place: `ReflectedFormView` (host) dispatches per-kind to
`ReflectedWidget`s registered in `ReflectedWidgetRegistry`, and `ReflectedWidgetHost` exposes the
services a widget needs (form/commands/skin/text/bounds/dirty/refresh/text-edit). Only the demo
consumes it. `IEditorPropertySource` / `RegisteredPropertySource` / `IEditorConfigSource` are declared
in `editor/core/property/EditorPropertySource.h`. `Enum` is the one kind still drawn/edited inline.

## Goals / Non-Goals

**Goals**

- Enum becomes a widget so the view keeps only layout/container behavior.
- Inspector renders the selection through the framework + a selection source.
- A global-config panel renders named world configs through the framework + a config source.

**Non-Goals**

- Rich asset/colour extensions beyond what exists.
- New themes, performance work, or the legacy Qt editor.

## Decisions

### D1. `EnumReflectedWidget`
Move the enum dropdown (open/select/hover/close + escape) into a widget registered for
`PropertyEditorKind::Enum`; the view's `BeginInteraction` loses its enum case and the inline enum popup
is deleted. Popup state lives in the widget (like `AssetReflectedWidget`).

### D2. Inspector via a selection source
`InspectorPanel` becomes (or hosts) a `ReflectedFormView`; on selection change it resolves the selected
item through an `IEditorPropertySource` and calls `Bind`. The default `RegisteredPropertySource` maps a
`Uuid` to published `PropertyObject`s, so extensions/world systems publish inspectable data. Empty
selection -> empty form.

### D3. Global config via a config source
`IEditorConfigSource::GetConfigs()` returns `NamedPropertyObject`s; the panel renders one collapsible
section per configuration through the existing `ReflectedFormView` (bind each in turn, or a small
sectioned variant). The module implements it over `World`'s named configurations.

### D4. Write-back needs a mutable config
`World::GetConfigByName` returns `const Any&`; add an additive non-const accessor
(`GetMutableConfigByName`) used only by the editor config source, so edits go through the descriptor
setter + `CommandService` as usual.

### D5. Module wiring
The sandbox module injects the selection source and config source, and registers the global-config
panel id + default layout (next to the existing inspector panel).

## Risks / Trade-offs

- [World config storage mutability] -> additive accessor, runtime path unchanged.
- [Multi-object selection] -> inspector shows the first non-empty resolution (existing behavior).
- [Panel vs view] -> if `InspectorPanel` already exists as a placeholder, replace its body with a
  `ReflectedFormView` subclass; keep the panel id stable.

## Migration Plan

Additive: add `EnumReflectedWidget`, remove the inline enum code, wire the two panels, inject sources in
the module. Existing demo keeps working.

## Open Questions

- Whether the global-config panel stacks sections in one view or uses a tab per config (default: stacked
  collapsible sections).
