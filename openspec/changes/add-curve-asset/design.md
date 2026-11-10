## Context

Curve introduces a reusable core evaluator, source/product assets, cook module and editor without changing Particle or Animation asset formats. The architecture review found unsafe input boundaries and duplicated document/interaction state. This change also closes those gaps using the existing EditorCore, EditorWidgets and UI targets.

## Goals and scope

- Keep `Curve -> Core`; asset code may depend on Framework, and tooling depends on the asset/core APIs.
- Ship linear, step and cubic Hermite interpolation, separate pre/post extrapolation, tangent editing, multi-selection and undoable authoring.
- Reject invalid input before fixed-size writes or unbounded file allocation; preserve previous valid output on failure.
- Share document persistence/checkpoint mechanics across Curve, Particle and Animation; codecs and domain compilation remain local.
- Preserve current directories and the single metadata owner. Particle/Animation curve format migration, gradients, LUTs and new runtime consumers remain outside this change.

## Data and input contracts

Keys contain normalized time in `[0,1]`, value and incoming/outgoing tangent vectors. Channels are `1..4`; interpolation is LINEAR/STEP/CUBIC and pre/post wrap is CLAMP/REPEAT/MIRROR/LINEAR. The source and binary product retain their first, unpublished version 1 layout.

`ParseCurveSource` checks the 256 KiB source limit and channel range before reading component arrays, uses iterative JSON parsing and publishes the result only after full validation. Builder checks stream length before allocating the source buffer. Enum source tokens and reflection labels share private tables in the asset implementation.

All evaluation branches normalize unused channels to zero. Double intermediates avoid overflow when the final float result is representable. Results whose mathematical magnitude exceeds float range are not promised finite. Wrap uses the normalized `[0,1]` domain; samples between a domain endpoint and the first/last key clamp to that key.

## Ownership and target boundaries

| Target | Public contract | Implementation dependencies |
|---|---|---|
| Curve | Core data and evaluator | Core |
| Curve.Assets.Static / Curve.Assets | Product/source API | Curve, Framework |
| Curve.Builder.Static / Curve.Builder | Builder registration | Curve assets, Framework |
| Curve.Editor.Static / Curve.Editor | Editor document and view | EditorCore, EditorUi, EditorWidgets, Curve assets |
| EditorWidgets | Reusable CurveEditorWidget | Curve core only; no Curve builder/editor |

`Curve.Assets` owns reflection and the asset handler in the module definition phase. Runtime consumers must both link the asset API and declare the metadata owner's module dependency. Linking a static implementation library alone does not activate its module. Builder/editor are developer facilities; no new Curve-specific build switch is introduced.

## Document persistence

`SourceDocumentStorage` in EditorCore provides bounded reads through AssetDataBase paths, exclusive staging writes and atomic replacement, explicit FileOperationStatus errors, and deferred cook requests. Existing domain storage names alias this contract, retaining injectable storage in tests.

`SourceDocumentState` is a composed value, not a document inheritance hierarchy. It owns the canonical saved checkpoint, loaded/pending-save flags and revision. A successful load installs validated domain data and its canonical checkpoint, then clears history. Failed load/save preserves the draft and last successful checkpoint. Save requires a successful load, and repeated requests coalesce until the host tick. Domain-specific parsing, diagnostics and compilation remain in each document.

Curve exposes read-only current data and creates undo commands for mutations. No-op changes do not add history. The widget has one command factory and one current-snapshot callback; it executes commands through the provided CommandService.

## Editor interaction and reuse

Selection is a sorted set of valid indices; the primary index is stored separately. A gesture owns a temporary snapshot and stable coordinate range. History changes, model replacement and structural commands end the gesture before using a different key array. Captured MOVE/UP/CANCEL events continue outside graph bounds.

The key list uses the existing virtualized ListView. Inspector editing uses ReflectedFormView. Command metadata supplies EditorAction accelerators, enabled/eligible predicates and checked state, so menus, toolbars and shortcuts share one dispatch contract. Graph styling comes from UiTheme.

The document's optional `CreatePreview` result is the sole source of preview capability. Null means full-width content with no reserved preview background; a returned widget retains the preview column. No separate provider flag is added.

## UI command batching

UIPaintContext coalesces adjacent commands only when index ranges are contiguous and clip, texture and shape state match. Rounded shapes and line capsules retain their SDF parameters in vertices. Batching does not reorder draws or cross state changes. The 384 segments of a four-channel curve can share one command when their render state is identical.

## Related module work

`refactor-asset-tool-module-config` owns shared host manifest parsing, AssetTool cook/audit selection, static factory availability versus activation, and build/deployment wiring. This change consumes that manifest for Curve modules rather than adding another host-specific registration mechanism.

## Validation

- Source limits, excessive channels, deep JSON, enum boundaries and failure preserving output.
- Exterior samples, unused channels and representable extreme-value interpolation.
- Canonical dirty checkpoints, successful/failed reload, save failure/retry and no-op edits.
- Actual widget pointer/history interactions, sorted multi-selection, capture release, list scrolling and action eligibility.
- Optional preview layout and contiguous draw batching across state changes.
- Affected implementation libraries, modules and tests; source ownership check and strict OpenSpec validation.

Only actual commands/results from the current fix pass are recorded as current evidence in verification.md. Commit and archive remain separate user-confirmed actions.
