## Why

The shared-header refactor removed C++'s dependency on shader compilation. Subsequent work added owner namespaces and source bundles, but the scene path still flattens material techniques into fixed programs and Batch data. Standalone shader deployment is not integrated with project resource registration or packaging. Prior tests validate those mechanics, not the requested architecture.

ScenePass must match technique Tags into RenderQueues; FullScreenPass and ComputePass select techniques explicitly. Global/Pass/Batch remain the resource tiers, View data belongs to Pass, and materials plus Features supply ScenePass Batch inputs. Each module registers its resource rules once for development, cook and packaging.

## What Changes

- Keep owner-scoped shared `.h` declarations authoritative for C++; export persistent, version-controlled `.slangh` assets through CMake tooling, with deterministic updates and read-only synchronization checks independent of Slang.
- Preserve all material techniques and select them by RenderQueue Tag. FullScreenPass/ComputePass use explicit techniques and may omit Batch; program stage/entry names are explicit.
- Compose ScenePass Batch from material and Feature providers with explicit binding ownership, transactional validation and actual reflected layouts. Remove implicit SceneForward selection and the fixed scene Batch payload contract.
- Merge optional camera/View shader data into Pass, remove Global camera duplication, and retain independent values for each view/Pass invocation.
- Declare shader roots, exports, inventories and dependencies at plugin/module registration, sharing common resource-import metadata with assets while preserving their different identities and import semantics.
- Collect all Aurora shader assets under `engine/aurora/assets/shaders/` and each plugin's assets under `plugins/<plugin-id>/assets/shaders/`. Preserve domain-relative header paths when replacing `.h` with `.slangh`; compose configurable IFileSystem providers through existing MultiFileSystem after atomic path/collision checks.
- Collect the same resource graph through project cook/package and runtime product lookup, aggregating all selected shader records per target rather than hiding module indices through mount ordering.
- Retain existing Aurora targets, generic binding helpers, feature assembly, immutable source/frame snapshots and module leases.
- Remove unconsumed legacy shader/material/vertex assets, runtime HLSL block generation and obsolete build definitions; reconcile current documentation and superseded stage records after migration.

## Capabilities

### Modified Capabilities

- `aurora-shader`: C++-authored shader-header exports, unified resource registration, canonical paths, runtime/cook inputs and snapshot lifetime.
- `aurora-renderer`: queue-selected and explicit techniques, material/Feature Batch composition, View-in-Pass ownership and explicit pipeline assembly.

## Impact

SharedShader/export tooling, shader CMake helpers, plugin/module resource metadata, AssetDataBase/adaptor integration, ShaderFileSystem/ShaderResolver, material technique transport/selection, Pass binding, existing cook/cache facilities, AssetTool/project packaging and runtime product lookup. No new production target, Aurora business plugin or renderer hierarchy is proposed. Concrete directory relocations require separate review.

## Non-goals

- Do not generate complete scene material programs from Pass declarations or maintain a second editable shader ABI; persisted exports are generated assets updated with their authoritative `.h`.
- Do not create a second technique library, VFS, archive format, plugin loader or submission system.
- Do not make Aurora Core depend on Framework/plugin discovery, C++ consumers depend on Slang, or offline cook load rendering runtime DLLs.
- Do not convert path-based shaders into UUID assets, introduce extra descriptor tiers, remove CPU View scheduling or expand unrelated backend capabilities.
- Do not mix broad directory moves or a general hot-reload system into functional corrections.

## Status

2026-10-22: the six reviewed decisions, approved 13-file migration and obsolete-content cleanup are implemented, with Windows validation recorded in tasks.md. This checkpoint is ready for macOS validation; a stripped native shipping host and native Metal/macOS acceptance remain open. The change remains active and unarchived.
