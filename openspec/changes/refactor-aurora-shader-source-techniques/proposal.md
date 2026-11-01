> Status — 2026-11-01: active；typed V/P/S 实现已交付，仅余6.4的 macOS native Metal 与 stripped-products 验收，尚未取得该项完成证据。
> 可把这项精确验收移交 `refactor-aurora-target-shader-artifacts` 后统一收口；在正式移交前仍由本 change 保留，不能以 iPad 示例或普通编辑器启动代替。

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

- `aurora-shaders`: unified selection, logical/target metadata and stage-scoped specialization.
  selection/code/executable identities and generated product metadata.

- `aurora-resources`: generated parameter keys, one cbuffer and inactive semantics.

- `aurora-renderer`: coherent material/scene preparation and migration.

- `asset-cook`: normalized demand and compiler-free published selections.

## Impact

Existing Aurora core, shader, pipeline, adaptor, cook, editor and native backend code; shader fixtures, product codecs and documentation. No new or removed target, loader, plugin or directory hierarchy. Shared declarations and .slangh exports remain authoritative. MaterialInstance static overrides, arbitrary Material storage buffers and material graphs are outside scope.

The fixed-compiler declaration/mapping gate precedes the main refactor. Native Metal execution remains an explicit platform acceptance requirement, not a cross-compilation claim. Commit and archive require separate confirmation.
