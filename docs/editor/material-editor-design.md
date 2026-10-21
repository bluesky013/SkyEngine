# Material Editor View

Status: current implementation record (2026-10-22). The earlier proposal to replace
StackPanel with direct row painting and hard-coded row heights is superseded. It
was not implemented and is not the current UI contract.

## Current implementation

`engine/aurora/editor/src/MaterialEditorView.cpp` edits `.mat` / `.mati` data loaded
through MaterialReflection. `MaterialEditorRow` stores row data; a one-column
`GridPanel` supplies layout slots. `RebuildRowSlots()` updates those slots when the
row list changes, and `ArrangeChildren()` uses `UiTheme.metrics.rowHeight`.
Painting and hit-testing consume the arranged row rectangles and existing UiSkin.

Technique rows expose Tag plus explicit vertex/fragment source and entry fields.
The material version-2 format preserves uniform mappings and variant values during
JSON roundtrip; those fields currently require JSON authoring. Renderer support is
narrower than the editable render-state schema: the scene path supports depth/blend
and rejects unsupported raster/stencil/alpha-to-coverage requests.

See [shader authoring and migration](../aurora/shader-design-and-migration.md) for
current resource paths, technique selection and material field mappings.

## Remaining UI work

Technique/render-state collapse is not implemented. There are no
`collapsedTechniques` or `collapsedRenderStates` collections in the current view.
Any future collapse/editing work must respect the editor's UI-tree mutation rules,
use UiTheme/UiSkin, and reuse the reflected editing/CommandService infrastructure.
The old proposal does not establish a current crash or authorize replacing layout
widgets with hard-coded geometry.

The shader/resource cleanup updates this record only; it does not implement a
material-editor redesign. Manual add/delete/edit/collapse acceptance must be
recorded with any future UI change, separately from shader and asset-codec tests.
