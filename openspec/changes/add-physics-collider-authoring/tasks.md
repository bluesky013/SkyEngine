# Physics asset, streaming and authoring tasks

Status (2026-10-27): the runtime asset/cell-loading slice and benchmark cases are implemented and verified. Unchecked items are subsequent iterations, tracked with acceptance criteria in [remaining-work.md](remaining-work.md). They do not mean that the already delivered explicit streaming host is unimplemented. This change remains open; no archive is requested in the current batch.

## 1. Design review and implementation boundary

- [x] 1.1 Inspect the current physics, asset, reflection, document, action and viewport interfaces.
- [x] 1.2 Compare official Unreal, Unity and Godot workflows and current convex-decomposition tooling.
- [x] 1.3 Produce proposal, detailed design, capability scenarios and phased implementation tasks; clearly identify design-only status.
- [x] 1.4 User confirmed material/Collision assets, builder and Framework streaming host implementation on 2026-10-27; dedicated authoring-editor implementation remains planned.
- [x] 1.5 Compare official Unreal, Unity and Godot resource/streaming lifecycles; document material ownership, preparation versus activation, retirement and stable scene identities.
- [x] 1.6 Record the approved Framework explicit cell-host boundary and integrated validation in sections 7/8 and verification.md; keep spatial scheduling and Terrain integration in subsequent tasks 6.5a/6.6a.

## 2. M1 data and product path

- [x] 2.1 Add local shape rotation with legacy defaults and a product-to-activation/query regression.
- [ ] 2.1a Complete the authoring validation matrix for nested compound transforms, COM, nonuniform scale and query agreement.
- [x] 2.2 Add version 1 Collision source/product codecs, stable element IDs, version validation and distinct registration owners.
- [ ] 2.2a Extend authoring recipes with source-node identities, dependency/content fingerprints, build keys and stale-product package rejection.
- [ ] 2.3 Add or extend the consumer-owned geometry-source contract and provider required by offline fitting/import; reuse existing Physics mesh interfaces where suitable and keep plugin schema out of engine.
- [x] 2.4 Implement builder product publication without loading runtime adaptor; add a generic byte-product publishing API only if the existing pipeline requires it.
- [ ] 2.5 Generate box/sphere/capsule fits, single hull and static mesh from selected model inputs with structured validation and budgets; consuming pre-authored geometry products is already delivered.
- [x] 2.6 Add explicit Inline/Asset component mode, body-level material resolution and transactional shape installation preserving constraints on failure.
- [x] 2.7 Add versioned PhysicsMaterial source/product codecs, one metadata owner, body asset references and package dependency collection; preserve default material compatibility.

## 3. M1 editor integration

- [ ] 3.1 Register CollisionDocument via AssetViewProviderRegistry and DocumentService with source storage, atomic save and cook status.
- [ ] 3.2 Register reflected authoring settings, asset fields and actions; bind shared forms and use shared toolbar/menu/icon/theme facilities.
- [ ] 3.3 Implement shape selection, transform/size handles, one-command drags, Escape cancellation and undoable generation application.
- [ ] 3.4 Add isolated physics preview with reset/pause/single-step and engine debug geometry; prewarm shapes outside rendering and simulation callbacks.
- [ ] 3.5 Implement revision-checked async work, cancellation, single completion and leased module/document lifetimes.
- [ ] 3.6 Exercise multi-instance editing, independent copies, asset failure, undo/redo and editor restart through contract tests and bounded UI smoke.

## 4. M2 import and convex decomposition

- [ ] 4.1 Add explicit DCC helper mapping, stable source node identities and reimport conflicts that preserve manual edits.
- [ ] 4.2 Spike a pinned CoACD revision: dependency/license audit, exception paths, cancellation, quality, runtime and peak-memory measurements.
- [ ] 4.3 Implement isolated decomposition with reflected parameters, budget/quality status, deterministic build keys and cleanup on worker failure.
- [ ] 4.4 Add batch generation/cook, stale-product packaging rejection and target-bundle regression tests.

## 5. M3 validation and follow-on decisions

- [ ] 5.1 Run the design's geometry, editing, cooking, physics and lifetime validation matrix on project fixtures.
- [ ] 5.2 Measure native shape build time and cache behavior; decide whether a separate native-product proposal is warranted.
- [ ] 5.3 Validate supported shipping platforms and no-tool runtime deployment; record untested targets honestly.
- [ ] 5.4 Review whether complex query geometry and per-face materials are needed as separate interface changes.

## 6. M1 physics streaming contracts and host integration

- [x] 6.1 Reuse AssetManager/AssetExecutor for ProductsOnly asynchronous geometry/material preparation; validate Jolt worker safety and prohibit blocking component attachment.
- [x] 6.2 Add immutable prepared-shape caching keyed by product snapshot identity, scale, margin and SDK session, with in-flight deduplication, per-consumer cancellation, SDK/code leases and accounted source-byte/concurrency limits. Content fingerprint semantics remain in 2.2a; complete native-memory accounting remains in 6.8a.
- [x] 6.3 Add Ready/Active/Retiring results and safe-point instance batches; preflight capacity, avoid partial query visibility and roll back only new candidates on failure.
- [x] 6.4 Track world/owner generations and product revisions through cancellation, world destruction and hot replacement; collect all tasks before SDK release.
- [x] 6.5 Implement the Framework explicit host with stable cell/actor identities, Ready/Active/Retiring states, current constraint/support pins and dynamic-object promotion.
- [ ] 6.5a Add host-owned cell bounds/product locations/dependency groups, spatial demand, priorities, prefetch/unload hysteresis, time budgets and explicit teleport/spawn gating; do not create a Jolt partition scheduler.
- [x] 6.6 Integrate incremental inline World cell manifests, legacy single-world loading, unloaded-cell preservation and complete asset-root collection.
- [ ] 6.6a Review and integrate Terrain pager preparation/activation/residency acknowledgements independently of render LOD and visibility.
- [x] 6.7 Verify shared resources, cancellation/retry, capacity rollback, moving-body promotion, cross-cell constraints and current-contact support retention using the explicit World host.
- [ ] 6.7a Exercise real spatial/teleport policy and Terrain residency together, including high-speed entry, missing coverage, pin exhaustion and stable cross-cell endpoint resolution.
- [x] 6.8 Add bounded repeated-paging/mesh/cancellation benchmarks and verify fresh-World scene references and source-free package closure; see sections 7/8.
- [ ] 6.8a Profile the observed preparation-Tick spikes before changing code, add optimized-build and long-duration native/process-memory baselines, and validate configured host commit budgets on declared hardware/workloads.

## 7. Implemented asset/streaming slice (2026-10-27)

- [x] 7.1 Add version 1 standalone geometry/material codecs, stable element IDs, atomic products and ProductsOnly runtime consumption.
- [x] 7.2 Add reflected component source/material fields, body-owned damping and local Shape rotation; preserve legacy inline serialization.
- [x] 7.3 Add incremental World cell manifests, initial targets, bounded preparation/activation, atomic physics retirement and unloaded-cell root collection.
- [x] 7.4 Cover cancellation, old product snapshots, world shutdown, group failure, dynamic promotion, constraint/support pins and repeated shared-resource paging.
- [x] 7.5 Finish independent native cook/package audit, editor regression and final build verification; record results in verification.md.

The unchecked M1 authoring-window, fitting/import and terrain/spatial-policy tasks remain separate follow-on work; this implementation does not claim the entire proposal is complete.

## 8. Asset/streaming benchmark cases (2026-10-27)

- [x] 8.1 Reuse JoltPhysicsTest and the existing asset/World fixture for shared products, unique shapes, resident reuse, triangle meshes and cancellation/reload benchmarks.
- [x] 8.2 Export per-phase wall and host-Tick distributions, measured polling/drain time, workload sizes and sampled shape-resource counters through gtest properties.
- [x] 8.3 Keep heavier benchmarks opt-in; assert readiness/query visibility, body counts, sharing and retirement cleanup without hardware-specific speed thresholds.
- [x] 8.4 Run the ordinary regression and explicit benchmark selection, record the Debug baseline and document reproduction and measurement limits in benchmarks.md.

Optimized-build baselines, large-scene sustained native/process memory profiling and shipping-platform measurements remain open in 6.8a/5.3. The task-to-spec acceptance map is remaining-work.md.
