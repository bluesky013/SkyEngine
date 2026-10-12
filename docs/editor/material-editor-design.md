# Material Editor View — design

Status: design (implementation of collapse not started). Supersedes the P5 "StackPanel rows" migration
for this view.

## Problem

`MaterialEditorView` (custom asset-view content for `.mat` / `.mati`) builds **one child `UIElement`
per row** inside a vertical `StackPanel` (the P5 layout migration), and rebuilds those children
whenever the row set changes (`ReloadTechniques()` → `RebuildRowSlots()` → `ClearChildren()` + `Add`).
Collapsing / adding / deleting rows therefore **destroys and recreates UI elements**. Doing that from
inside pointer/key event dispatch (or even deferred to `OnPaint`) **hangs / crashes** — building on
the "do not rebuild the UI tree mid-dispatch" rule (coding rule 9).

Root cause: for a **hand-painted row list**, per-row child elements are the wrong model.

## Reference — how the Inspector avoids it

`ReflectedInspectorPanel : ReflectedFormView` renders a reflected object by **painting rows directly**
in `OnPaint` (no per-row child elements). Collapsing a struct/sequence only flips
`PropertyField::expanded` and changes **which rows are painted** — no element creation/destruction, so
it is inherently safe. Hit-testing/editing uses rects computed from the current row list.

The material editor should follow the same model.

## Decision

**Paint rows directly; keep `rows` as pure data; never keep per-row child elements.**

1. Drop the `StackPanel` + per-row `UIElement`s. `rows` (a `std::vector<MaterialEditorRow>`) is the
   only layout input.
2. Row rectangles are computed each frame by a `RowRect(i)` helper from the view bounds + `kRowH`
   (the pre-P5 `ComputeRowRects` approach). `Measure()` returns total height from `rows.size()`.
3. Hover / edit / texture-picker hit-testing uses those computed rects.
4. Collapse is pure data: `collapsedTechniques` / `collapsedRenderStates` (`std::set<int>`) change
   **which rows `ReloadTechniques()` emits** (in memory only). No UI element is touched.
5. Because nothing in the UI tree changes on collapse / add / delete, calling `ReloadTechniques()`
   from event handlers is safe again (and we no longer need the deferred-reload hack).

## Row model

`MaterialRowKind`: `Field | TechniqueHeader | RenderPipelineHeader | AddTechnique`.

- **Property rows** (`Field`): label + typed value editor (variant) or texture picker.
- **TechniqueHeader** (`v `/`> ` triangle, `Technique i [tag]`, `x` delete): clicking the triangle
  toggles `collapsedTechniques[i]`; collapsing hides that technique's field rows.
- **RenderPipelineHeader** (`v `/`> `): groups a technique's `renderState` rows; toggles
  `collapsedRenderStates[i]`.
- **AddTechnique** (`+ Add Technique`).

## Data model (context)

Source `.mat` / `.mati` is loaded/saved through the reflection JSON codec on `MaterialAssetData` /
`MaterialInstanceSource` (see `MaterialSourceParse` removal + `MaterialReflection`). Property values
are a `std::variant` (`MaterialPropertyValue`). This view only renders/edits that data.

## Migration

1. Remove `rowsPanel` / `RebuildRowSlots` / `LayoutRows` (StackPanel slots).
2. Reintroduce direct row-rect computation (`RowRect`) + paint/hit-test against it.
3. Add the collapse row kinds + state (as above) with in-place `ReloadTechniques()` on toggle.
4. Delete the leftover `RenderStateHeader` dead code if unused.

## Verification

- Open a material; click a Technique / Render Pipeline header → folds/unfolds **without hanging**.
- Add / delete technique → no hang.
- Edit a value + texture pick → still works.
- `EditorShellTest` + `Aurora.AdaptorTest` stay green.
