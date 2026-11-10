## 1. Domain and module scaffolding

- [x] 1.1 Register the `curve` capability/domain in `openspec/README.md` and `openspec/capability-map.json` (status pending-archive, change `add-curve-asset`)
- [x] 1.2 Create `engine/curve/{core,assets,builder,editor}` with `CMakeLists.txt` per target and add the subsystem to `engine/CMakeLists.txt`
- [x] 1.3 Add `Curve.Assets` and `Curve.Editor` entries to `configs/modules_editor.json` with the correct dependencies; keep builder out of runtime module configs
- [x] 1.4 Declare `Curve.Builder` in `configs/modules_asset_tool.json`; use the shared manifest resolver for runtime activation and CMake dependencies

## 2. Core data and evaluation

- [x] 2.1 Define `CurveKey {time, Vector4 value}`, `CurveInterpolation` (`LINEAR`, `STEP`, `CUBIC`, `END`), `CurveWrap` (`CLAMP`, `REPEAT`, `MIRROR`, `LINEAR`, `END`) and `Curve` in `engine/curve/core`
- [x] 2.2 Implement `ValidateCurve` (ordered unique times in `[0,1]`, finite values, channels `[1,4]`, non-empty keys)
- [x] 2.3 Implement allocation-free `Curve::Evaluate(float)` with wrap mapping before sampling and the documented empty-key behavior
- [x] 2.4 Add `CurveTest` covering linear, step, clamp/repeat/mirror, empty keys, boundary keys and invalid descriptors

## 3. Curve asset and registration

- [x] 3.1 Define `CurveAssetData` (version, channels, interpolation, wrap, keys, source hash) and the `.curve` type/extension constants
- [x] 3.2 Implement the versioned source JSON reader/writer with field-level diagnostics and round-trip preservation
- [x] 3.3 Implement the binary product reader/writer with magic, format version, size validation and source hash
- [x] 3.4 Specialize `AssetTraits<Curve>` and register authoring reflection in `Curve.Assets` (single metadata owner)
- [x] 3.5 Register the asset handler and `.curve` extension mapping; reject duplicate registration explicitly
- [x] 3.6 Add `Curve.AssetsTest` covering source round-trip, rejected sources, product version mismatch and asset registration

## 4. Cook

- [x] 4.1 Implement `Curve.Assets` module registration and `Curve.Builder` module entry
- [x] 4.2 Implement the `.curve` builder: read source, validate/compile, write the product atomically, keep the previous product on failure
- [x] 4.3 Add `Curve.BuilderTest` for successful cook, invalid source, stale source hash and unchanged output on failure

## 5. Editor

- [x] 5.1 Implement `CurveDocument` (source load/save, revision, compile to `Curve`, undoable commands) and `CurveEditorDocument` implementing `IAssetViewDocument`
- [x] 5.2 Implement the content view: key list (add/remove/select), reflected form for the selected key and curve header (channels/interpolation/wrap), with inline validation
- [x] 5.3 Host the primary curve graph in the content region (no second renderer); the generic preview region is not used
- [x] 5.6 Extract the reusable `CurveEditorWidget` in `engine/sandbox/widgets` (interactive graph, key list, inspector)
- [x] 5.7 Use optional `CreatePreview` as the sole layout capability, with full content width when it returns null
- [x] 5.8 Add cubic tangents, pre/post extrapolation, multi-select/marquee, copy/paste/duplicate, snapping, axis labels and tangent handles
- [x] 5.9 Define the curve command table and register it as scoped editor actions with keyboard shortcuts
- [x] 5.4 Register the `.curve` asset view provider in `Curve.Editor` and make the asset openable by double-click; ensure save writes once and requests cook through the existing flow
- [x] 5.5 Add `Curve.EditorTest` for document load/save, undo history, invalid edit rejection and provider registration

## 6. Build, verification and close-out

- [x] 6.1 Build all curve targets plus `SandboxModule`, `AssetTool` and the curve tests on macOS
- [x] 6.2 Run `CurveTest`, `Curve.AssetsTest`, `Curve.BuilderTest`, `Curve.EditorTest`, `CMakeSourceOwnership` and `openspec validate add-curve-asset --strict`
- [x] 6.3 Cook a sample `.curve`, open it in the editor, edit a key and verify the preview and saved source
- [x] 6.4 Record verification evidence in `verification.md` (module load, cook, editor, save/cook flow, known follow-ups)

## 7. Architecture review fixes (2026-11-10)

- [x] 7.1 Validate source size/channels before decoding, use iterative JSON parsing and bound builder reads.
- [x] 7.2 Normalize all evaluator exits and avoid representable-result intermediate overflow.
- [x] 7.3 Share SourceDocumentStorage/SourceDocumentState across Curve, Particle and Animation; canonicalize checkpoints and preserve failed operations.
- [x] 7.4 Fix gesture/history/model lifetime, separate primary selection, reuse ListView and EditorAction accelerators.
- [x] 7.5 Remove duplicate preview capability and batch adjacent compatible shape commands.
- [x] 7.6 Build and run the affected implementation/modules/tests, sanitizer reproductions, ownership checks and strict spec validation; record current results.
