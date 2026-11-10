## ADDED Requirements

### Requirement: Curve data model

The `Curve` core type SHALL store an ordered key list with `time` in `[0, 1]`, a `Vector4` value and per-channel `tangentIn`/`tangentOut` slopes, plus a channel count in `[1, 4]`, an interpolation mode (`LINEAR`, `STEP` or `CUBIC`) and separate pre/post extrapolation modes (`CLAMP`, `REPEAT`, `MIRROR` or `LINEAR`). It SHALL depend only on `Core`.

#### Scenario: Valid curve description

- **WHEN** keys are ordered, finite and inside `[0, 1]`, and channels are in `[1, 4]`
- **THEN** validation succeeds and the curve can be evaluated

#### Scenario: Invalid curve description

- **WHEN** keys are unordered, non-finite, outside `[0, 1]`, or channels are outside `[1, 4]`
- **THEN** validation fails with an explicit error and no partial curve is produced

### Requirement: Deterministic evaluation

`Curve::Evaluate(float t)` SHALL be deterministic and free of allocations. Samples outside `[0, 1]` SHALL use the pre/post extrapolation mode: `CLAMP` clamps to the end keys, `REPEAT` wraps with `frac`, `MIRROR` reflects at the ends, and `LINEAR` continues the end-segment slope. `STEP` SHALL return the value of the last key at or before the sample time; `LINEAR` SHALL interpolate adjacent keys; `CUBIC` SHALL evaluate a Hermite segment using the outgoing tangent of the left key and the incoming tangent of the right key. An empty key list SHALL evaluate to the zero vector. All branches SHALL zero unused channels and avoid intermediate overflow when the final float interpolation result is representable.

#### Scenario: Linear interpolation

- **WHEN** a linear curve has keys at `t = 0` and `t = 1` with different values and is evaluated at `t = 0.5`
- **THEN** the result is the midpoint of the two values

#### Scenario: Step interpolation

- **WHEN** a step curve has keys at `t = 0` and `t = 0.5` and is evaluated at `t = 0.49`
- **THEN** the result is the first key value

#### Scenario: Wrap modes

- **WHEN** a repeat or mirror curve is evaluated with `t < 0` or `t > 1`
- **THEN** the sample time is wrapped or reflected before evaluation instead of clamping

### Requirement: Curve source asset

The source asset SHALL use the `.curve` extension and a versioned JSON document (version 1) containing `version`, `channels`, `interpolation`, `preWrap`, `postWrap` and an ordered `keys` array of `{time, value, tangentIn?, tangentOut?}`. The reader SHALL reject unknown versions, wrong value shapes, non-finite numbers, unordered keys and key times outside `[0, 1]`, and SHALL report the failing field. Writing SHALL round-trip supported fields of a valid document. The parser SHALL reject sources larger than 256 KiB and channel counts outside `[1,4]` before component writes, parse without recursive descent, and preserve the caller's previous curve on failure.

#### Scenario: Source round-trip

- **WHEN** a valid `.curve` document is read and written again
- **THEN** the resulting document is semantically identical and the compiled curve is unchanged

#### Scenario: Rejected source

- **WHEN** a document has version `2`, an unordered key, or a 5-component value
- **THEN** loading fails with a diagnostic and no product is registered

### Requirement: Curve product asset

The cooked product SHALL be a binary payload with a magic tag, a format version (1), the channel count, interpolation, pre/post wrap and key records including per-channel tangents. Loading SHALL reject mismatched magic or version and SHALL validate the payload before exposing it. The source hash SHALL be recorded so stale products are detectable.

#### Scenario: Product version mismatch

- **WHEN** a product was written with a different magic or format version
- **THEN** loading fails explicitly and existing valid products are not overwritten

### Requirement: Asset registration and dependency boundary

The curve asset SHALL register the type name `Curve`, an `AssetTraits` specialization with a binary serialize type, an asset handler, authoring reflection, and the `.curve` extension mapping. Consumers SHALL be able to depend on `Curve.Assets.Static` without linking cook or editor targets; `Curve` SHALL depend only on `Core`, and `Curve.Assets.Static` only on `Curve` and `Framework`.

#### Scenario: Runtime consumer links assets only

- **WHEN** a subsystem links `Curve.Assets.Static` and its host activates the `Curve.Assets` metadata owner
- **THEN** it can load and evaluate curve products without linking `Curve.Builder` or `Curve.Editor`

#### Scenario: Duplicate asset registration is rejected

- **WHEN** two modules attempt to register the `Curve` type or handler
- **THEN** the second registration fails instead of silently replacing the first

### Requirement: Cook pipeline

`Curve.Builder` SHALL register a builder for the `.curve` extension that reads the source document, validates and compiles it, writes the binary product atomically, and reports diagnostics without partial output. `AssetTool` SHALL load the builder module and cook `.curve` assets.

#### Scenario: Cook succeeds

- **WHEN** a valid `.curve` source is cooked
- **THEN** a product with the matching source hash is written and the asset can be loaded as `Curve`

#### Scenario: Cook rejects an invalid source

- **WHEN** the source is invalid
- **THEN** the builder reports the diagnostic, leaves the previous product untouched, and does not mark the asset as cooked

### Requirement: Curve asset editor

The editor SHALL open `.curve` assets through the generic asset viewer and provide a reusable curve editor widget: an interactive graph plus a key list and inspector in the content region. The graph SHALL support single and marquee multi-selection, dragging one or many selected keys (time/value, with optional snapping), pan/zoom/frame, Delete/Backspace removal, copy/paste/duplicate, adaptive major/minor axis ticks with labels, a cursor crosshair with time/value readout, and cubic tangent handles for the primary key. The inspector SHALL edit the selected key or the curve header (channels/interpolation/pre-post wrap). Curve commands (add, remove, duplicate, copy, paste, frame all, snap, interpolation) SHALL be defined once in a command table with enablement predicates and exposed to the host action registry and keyboard shortcuts. All mutations SHALL be undoable and routed through the host save/cook flow. The view SHALL NOT introduce a second renderer.

#### Scenario: Edit and save

- **WHEN** a key value is edited and the document is saved
- **THEN** one undoable edit is recorded, the source is written once, and the cook is requested through the existing asset flow

#### Scenario: Invalid edit

- **WHEN** a key time violates ordering or an edit would leave no keys
- **THEN** the edit is rejected and the last valid curve stays displayed and saved

### Requirement: Optional asset preview

The asset viewer SHALL use the document's `CreatePreview` result as the sole preview capability. A null result SHALL give the content the full body width and SHALL NOT paint a reserved preview background. A returned widget SHALL retain the existing preview column.

#### Scenario: Curve editor uses the full width

- **WHEN** the Curve document returns no preview widget
- **THEN** the graph/key list/inspector use the full body width

#### Scenario: Document supplies a preview

- **WHEN** a document returns a preview widget
- **THEN** the existing 38% preview column is retained without overlapping the content

### Requirement: Curve interaction lifetime

The editor SHALL cancel active interactions before history changes, model replacement or structural edits. Captured gestures SHALL receive release/cancel events outside graph bounds. Primary selection SHALL NOT change the ordering of the selected-index set.

#### Scenario: Undo while dragging a newly inserted key

- **WHEN** a key is being dragged and undo removes that key
- **THEN** the drag ends before rebinding, and later pointer movement cannot access the removed key

#### Scenario: Cancel and delete multiple keys

- **WHEN** a drag with a non-last primary key is cancelled and the selected keys are deleted
- **THEN** only those keys are removed, using valid descending indices

#### Scenario: Release outside the graph

- **WHEN** a captured drag is released outside the graph
- **THEN** editing ends and later hover does not continue the edit

### Requirement: Domain registration

The change SHALL register the `curve` domain in `openspec/README.md` and `openspec/capability-map.json`, and the change SHALL keep the runtime, cook and editor module entries consistent with `configs/modules_editor.json`.

#### Scenario: Domain lookup

- **WHEN** the capability map is inspected
- **THEN** the `curve` capability resolves to the `curve` domain and the pending change

#### Scenario: Module activation

- **WHEN** the editor starts with `SKY_DEVELOP` and `SKY_BUILD_SANDBOX`
- **THEN** `Curve.Assets` and `Curve.Editor` load and `.curve` assets are registered and openable
