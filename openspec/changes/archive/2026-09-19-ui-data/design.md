## Context

Builds on the archived `engine-ui-system` core (`engine/ui/core`) and `ui-widgets-core` (Panel/Image/Button).
The core already defines `UIElement` (tree/dirty/layout/paint/events), `UITheme`, `UIDrawData`, and the
`IUITextureRegistry` / `IUIFontProvider` / `IUIAssetResolver` seams. `openspec/specs/ui-data` specifies the
document layer; this change implements it without any render dependency so it can land while the Aurora pass
waits. `Core` already links `3rdParty::rapidjson`, so JSON parsing reuses an in-tree dependency.

## Goals / Non-Goals

**Goals:**
- A JSON document format and loader that build a `UIElement` tree through an extensible registry.
- A property model for applying document/type properties to elements without RTTI or `dynamic_cast`.
- A data-context + binding model with built-in converters and predictable evaluation timing.
- A C++ builder that produces the same tree and bindings as the loader.

**Non-Goals:**
- `Text`/`text.content` (no `Text` widget yet), event-graph bindings, editor/codegen, hot reload, binary cache.

## Decisions

### 1. Property value and `SetProperty`
Add `UIPropertyValue` (typed scalar: bool/int/float/string). `UIElement` gains
`virtual bool SetProperty(const std::string &path, const UIPropertyValue &value)` handling common paths
(`name`, `visible`, `enabled`); `Image`/`Button` override for `visual.texture`/`visual.tint` and
`button.interactable`. Unknown paths return false and become loader diagnostics. Alternative considered:
`dynamic_cast` per widget — rejected (repository forbids RTTI-based casting).

### 2. Element registry
`UIElementRegistry` maps a `type` string to `std::function<UIElementPtr()>`. Built-ins register `Panel`,
`Image`, `Button`. Unknown types produce an error diagnostic and are skipped. Alternative: hard-coded switch —
rejected because it blocks extension and later `ui-data` reuse.

### 3. Loader and diagnostics
`UIDocumentLoader::Load(json, registry, resolver)` parses the document and returns
`UIDocument { root, bindings, nameIndex, diagnostics }`. It never throws on content errors. Common and layout
props are applied by the loader; type props go through `SetProperty`. Texture refs resolve via
`IUIAssetResolver`; unresolved refs are warnings. Alternative: throw on bad input — rejected (runtime content
must degrade, not crash).

### 4. Data context, provider, and bindings
`IUIDataProvider` resolves a path to a `UIPropertyValue`. `UIDataContext` is the default provider: a named-value
map with a change version and change callback. `UIBinding` holds `{ element, targetPath, sourcePath, converter,
args }` and re-applies when the context version changes. Built-in converters: `remap` (linear in->out range),
`format` (numeric to string), `boolToVisible` (bool passthrough documented for visibility targets). Alternative:
a general reflection/VM system — out of scope; the provider seam leaves room for it.

### 5. C++ builder parity
`UIBuilder` wraps a `UIContext` and uses the same registry/factories to create elements, apply the same layout
props, and register the same bindings. Documents and code therefore produce equivalent trees; tests use the
builder to avoid file IO. Alternative: separate code paths — rejected (drift risk).

### 6. Source layout
Data-layer files live under `engine/ui/core/include/ui/data/` and `engine/ui/core/src/data/`, keeping them
separable from element base and widgets.

## Risks / Trade-offs

- **Binding paths are flat keys today** -> dotted paths are treated as opaque keys; a real path resolver is a
  follow-up behind `IUIDataProvider`.
- **Property coverage is partial** -> only paths with a widget implementation are settable; others warn. Grows
  with the widget set.
- **`rapidjson` becomes a direct UI dependency** -> acceptable; `Core` already vendors it and it is header-only.
- **Style-class to theme mapping stays last-wins** -> inherited limitation of the archived core.

## Migration Plan

Additive: new data sources, one new virtual on `UIElement`, one new linked dependency. Rollback removes the
data files. No render or Aurora changes.

## Open Questions

- Whether bindings should be owned by a `UIDocument` handle or by the `UIContext` for lifetime management.
- Whether `format` needs locale-aware formatting before localization lands.
- Whether sub-document/include nodes belong here or in a later composition change.
