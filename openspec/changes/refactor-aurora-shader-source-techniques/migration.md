# Migration and ownership

The confirmed design is recorded in design.md and docs/aurora/shader-variant-design-review-2026-10-25.md. This plan supersedes the withdrawn no-JSON/source-marker plan; its old deletion list and v3/v5 migration numbers no longer apply.

1. Validate the pinned compiler's key-to-symbol mapping, target declaration extraction and equivalent DXIL folding using existing test targets.
2. Introduce Core selection and per-stage target metadata, migrate callers and native specialization without creating parallel loaders or targets.
3. Generate stable Material schema and migrate existing properties/aliases through explicit mappings. Preserve values and diagnose conflicting/unknown legacy keys.
4. Update product codecs, usage/cook selection records and fixtures together. The previous baseline was index/blob v9, usage v2, Material v3 and instance v2. The implementation uses index/blob v10, usage v3, Material v4 and unchanged instance v2; the distribution manifest remains v2. Recook old shader products; legacy Material v3 is decoded and explicitly canonicalized against current declarations.
5. Update docs/features/shader-system.md, docs/aurora/shader-design-and-migration.md and affected domain instructions with the implemented contract. Main specs are synchronized through the authorized OpenSpec workflow; no archive occurs automatically.

refactor-aurora-target-shader-artifacts retains ownership of immutable target artifacts, native binding plans, uniform encoding and existing native validation evidence. refactor-aurora-pipeline-resource-ownership retains resource registration, shared exports and four-group ownership. This change extends their seams and does not mark their unexecuted tasks complete.

Keep ShaderCompanion, explicit ShaderRef stage identities, shared-header tools, resource inventories, compiler leases, immutable snapshots and existing task/cache writer infrastructure. No restoration of Slang technique markers, no removal of ShaderHeaderTool/targets and no directory cleanup is authorized by this change.
