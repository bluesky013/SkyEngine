## Why

Status: active; the user confirmed the reviewed V/P/S contract on 2026-10-25. The earlier source-attribute/no-JSON implementation was withdrawn on 2026-10-24 and is not resumed. The existing change identifier is retained to avoid overlapping ownership.

Scene, Batch, Fullscreen and Compute need one logical variant selection, while target compilation and native specialization need distinct identities. Current material-only naming, global specialization IDs, explicit material aliases and rejection of inactive values conflict with the confirmed contract.

## What Changes

- Preserve JSON companions and explicit stage path/entry programs. Normalize typed Vertex/Pipeline/Shader-local inputs in Core before target resolution.
- Separate complete selection, stage code and device executable identities. Derive per-stage native mappings from shader declarations and target capabilities; business inputs never specify macro/spec policy or native IDs.
- Generate complete Material parameter keys from stable declarations; Group 2 permits zero or one logical cbuffer. Retain inactive values and pack active fields using target reflection.
- Use the same preparation and selection contract for scene, custom batch, fullscreen and compute. Preserve Global/Pass/Material/Batch resource ownership independently of variant domains.
- Update cook/product metadata, requested-selection validation, existing tests and documentation together. Old incompatible products require recook; previous baseline was shader cache v9, usage v2, Material v3 and instance v2. Implemented formats are cache v10, usage v3, Material v4 and unchanged instance v2.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-shader`: unified selection, logical/target metadata and stage-scoped specialization.
- `aurora-shader-cache`: selection/code/executable identities and generated product metadata.
- `aurora-material`: generated parameter keys, one cbuffer and inactive semantics.
- `aurora-adaptor`: coherent material/scene preparation and migration.
- `aurora-cook`: normalized demand and compiler-free published selections.

## Impact

Existing Aurora core, shader, pipeline, adaptor, cook, editor and native backend code; shader fixtures, product codecs and documentation. No new or removed target, loader, plugin or directory hierarchy. Shared declarations and .slangh exports remain authoritative. MaterialInstance static overrides, arbitrary Material storage buffers and material graphs are outside scope.

The fixed-compiler declaration/mapping gate precedes the main refactor. Native Metal execution remains an explicit platform acceptance requirement, not a cross-compilation claim. Commit and archive require separate confirmation.
