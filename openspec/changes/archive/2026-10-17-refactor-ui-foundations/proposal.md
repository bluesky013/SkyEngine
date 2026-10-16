## Why

Aurora UI needs clear contract, runtime, editor, font-provider and renderer targets, together with an arrangement stage that prepares geometry before paint. The original mixed targets and paint-time child arrangement make reuse and first-frame input unreliable. By the user's explicit foundation exception, UI implementation remains in `engine/ui`; dependency separation is enforced between targets within that foundation module.

## What Changes

- Keep consumer-owned provider interfaces, their data, and pure layout utilities in the root `UI.Interface` target; keep the retained toolkit in `engine/ui/core` as `UI`, preserving `ui/...` includes.
- Keep the Aurora renderer separately in `engine/ui/render`, preserving `UIRender` and narrowing it to `UI.Interface` plus the Aurora RHI/shader compiler; isolate the optional FreeType provider as `UI.FreeType` in `engine/ui/freetype`.
- Isolate the headless document editor in `engine/ui/editor` as `UI.Editor`; separate its tests from runtime UI tests.
- Add `UIElement::Arrange(finalRect)` as the common entry for a parent's resolved child rectangle in the existing absolute surface coordinates, and run recursive StackPanel, GridPanel, DockPanel, and Overlay arrangement during layout.
- Remove layout work from these containers' paint callbacks and migrate affected Sandbox and material-editor compositions to arrange before reading geometry, routing input, or painting.
- Preserve the existing `Measure(float &, float &)` and anchor/box-size APIs. Constraint-based measurement, a full Geometry model, cache redesign, editor widget extraction, document instancing, and theme unification remain subsequent work.
- **BREAKING**: Code using `UIDocumentEditor` or `FreeTypeUIFontProvider` must explicitly link `UI.Editor` or `UI.FreeType`; `UI` no longer brings these implementations transitively.

## Capabilities

### New Capabilities

None. This change strengthens existing UI contracts.

### Modified Capabilities

- `ui-core`: render-agnostic foundation core, explicit resolved-rectangle arrangement, and paint/layout separation for slot containers.
- `ui-render`: Aurora adaptor placement and the compatibility of the `UIRender` target.
- `ui-text`: an optional, separately linked FreeType adaptor with a dependency-free fallback in core.
- `ui-editor`: document-editing module isolation from runtime UI.
- `engine-plugin-layering`: the explicit UI foundation exception and one-way contract/runtime/editor/backend target boundaries within `engine/ui`.

## Impact

- `engine/ui` foundation targets and their consumer dependencies. No UI feature-plugin entry is required.
- `engine/test/ui`: runtime, text, FreeType, and document-editor test targets.
- Sandbox layout consumers and the Aurora material editor; their existing public widget APIs and `ui/...` include paths remain available.
- No persisted document-format migration or renderer protocol change is required. The follow-up architecture work is recorded in `design.md` rather than included in this change's delivery tasks.

Placement revision (2026-10-17): active artifacts now describe the user-directed foundation layout. Earlier recorded verification used the preceding plugin placement; its counts remain historical evidence. Current relocation verification belongs to `simplify-ui-foundation-boundaries` and canonical §19.
