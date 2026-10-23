Status: active。阶段 A（1–4）15/15 与阶段 B（5–9）20/20 的原验收记录见 `verification.md`；第 10 节几何查询扩展接口 3/3 已实现验证，不实现具体业务后端。第 11 节保留本轮“仅扩展接口”范围之外的 review 待修项；合计 38/39。阶段 C 的 GPU 后端不在本任务清单内；本 change 尚未 commit/archive。

## 1. Contracts and geometry

- [x] 1.1 Add IViewportPicker and owned neutral query/result/status data in EditorCore; define context revisions and SelectionService application guards.
- [x] 1.2 Implement ray/bounds/triangle queries with clipping, supported culling and world-distance sorting; test holes, overlaps, parent transforms, non-uniform/negative scales and degenerate/singular inputs.
- [x] 1.3 Implement revision-keyed shared mesh BVH, bounded cache and recoverable preparation states; test reuse, eviction, reload and stale preparation results.

## 2. Existing adaptor integration

- [x] 2.1 Verify MeshAssetData / SceneAssetSource / RenderSceneBridge ownership, visibility, layout, material culling and revision data; document the exact supported static mesh/material subset.
- [x] 2.2 Expose owned neutral static mesh snapshots through the existing Aurora adaptor boundary, preserving Actor/part identity and geometry revision without Sandbox dependencies.
- [x] 2.3 Implement the CPU picker in the existing EditorRender target and compose it in SandboxModule; avoid duplicate decoding, synchronous asset loads in input callbacks and new modules/targets.
- [x] 2.4 Verify Pending/Unsupported/Error feedback and conservative nearest-hit completeness when relevant candidates are unresolved; keep unrelated unavailable assets from blocking valid queries.

## 3. Input and selection

- [x] 3.1 Add click-candidate tracking to SceneViewport with themed maximum travel threshold, matching button/pointer and in-content release; preserve Gizmo/navigation capture priority.
- [x] 3.2 Apply Hit/Miss through the existing SelectionService without World commands/dirty state; update Outliner, Inspector and Gizmo through their shared callbacks.
- [x] 3.3 Cancel candidates and reject stale results on focus, modal/menu, tool, layout/DPI/surface, camera/scene revision, document, target and Play transitions.
- [x] 3.4 Add main/floating Shell routing tests for supported selection, empty click, over-threshold return, competing gestures, pending completion, deleted targets and switched documents.

## 4. Verification and review

- [x] 4.1 Close editor, configure/build affected existing targets including SandboxModule, then run core/shell/render/UI and relevant adaptor regressions.
- [x] 4.2 Run native Vulkan and DX12 acceptance with overlapping meshes, bounds holes and a collider-free Actor; verify selection consumers, undo isolation and floating viewport coordinates.
- [x] 4.3 Measure cold preparation, warm query latency and cache memory on recorded triangle/instance counts; record platform and material/deformation limitations separately from passing evidence.
- [x] 4.4 Review module boundaries, error/lifetime paths and diff; validate OpenSpec and record actual results. Commit and archive remain subject to explicit user confirmation.

## 5. Request and result contracts (phase B)

- [x] 5.1 Extend neutral request data with viewport-local logical position, image pixel/extent mapping, an immutable camera/view snapshot and policy; keep renderer-native IDs and matrices conversion inside the adapter. Test main/floating DPI and render-scale mapping.
- [x] 5.2 Add bounded Submit/Poll/Cancel lifecycle with one resumable active request per viewport, immediately replaced by its newest request; preserve context validation and distinguish stale/cancelled ownership from user-visible failure.
- [x] 5.3 Add versioned target identity, hit semantics, optional surface position/normal and deterministic resolution of world surfaces, depth-tested proxies and explicitly overlaid proxies. Continue selecting only the owning Actor.
- [x] 5.4 Add policy/occlusion tests: filtered or locked occluders, unknown nearer bounds, equal-distance hits, unsupported VisibleSurface requests and cancellation with no completion applied.

## 6. Reusable mesh acceleration (phase B)

- [x] 6.1 Instrument decoding, primitive bounds/centroids, partitioning, allocation and publication separately; reproduce the phase A workload and establish repeated-run cold/warm distributions before changing the builder.
- [x] 6.2 Implement a measured binned-SAH builder with cached primitive data and reusable bounded scratch, deterministic balanced fallback, degenerate handling and cancellation; preserve Core BVH consumers and add brute-force differential tests.
- [x] 6.3 Separate CPU geometry identity from material and transform identity; document device-epoch reuse only when a producer guarantees identical owned bytes/layout. Test shared mesh reuse across different material contracts and revisions.
- [x] 6.4 Bound active/preparing/retiring mesh bytes and temporary payload ownership; check cancellation throughout decoding/bounds/partition/recursion; test pre-build and mid-bounds cancellation, cache pressure, retry and shutdown without partial BVH publication.

## 7. Instance index and incremental maintenance (phase B)

- [x] 7.1 Extend the existing scene extraction snapshot with stable instance identities and separate geometry/transform/capability/readiness revisions plus validated deltas; do not add a second component traversal.
- [x] 7.2 Add an EditorRender-private instance AABB tree with cached inverse/world bounds, incremental insert/remove/refit and near-first traversal; retain explicit records for candidates without conservative bounds.
- [x] 7.3 Implement bounded host updates with an incrementally height-balanced tree, suspend queries during partial maintenance, and restart generation-bound cursors; handle interrupted/lost delta continuity with full resynchronization. Record the measured decision to defer background rebuild/overlay.
- [x] 7.4 Test unchanged/camera-only updates, parent transform fan-out, sparse movement, topology churn, pathological overlap and unknown bounds; compare results to exhaustive traversal and record actual visited-leaf counts.

## 8. Capability and provider integration (phase B)

- [x] 8.1 Replace the geometricPicking boolean with versioned producer-owned surface facts and conservative defaults; support explicitly declared authored static opaque geometry without shader-name inference or asset schema expansion.
- [x] 8.2 Define IViewportPickProvider and scoped host registration in EditorCore; implement owned snapshot collection and composition in existing EditorRender/SandboxModule, with duplicate-ID validation and unload/cancellation tests.
- [x] 8.3 Implement typed triangle, world-segment and billboard-rectangle proxies with projection/tolerance rules, bounds and depth policy; keep transform Gizmo capture ahead of scene selection and reuse provider layout data for visual/hit agreement.
- [x] 8.4 Add integration fixtures for an authored material with explicit capability, a non-mesh icon/line provider, unsupported deformation and two providers competing at equal depth; verify Actor selection consumers and no World dirty/undo changes.

## 9. Phase B verification and review

- [x] 9.1 Run deterministic oracle, stale-token, provider unload, memory-pressure and bounded-query tests; use counters for structural regressions and separate non-flaky performance reports.
- [x] 9.2 Measure cold build and 200/10,000/100,000-instance queries, sparse updates, moving-camera and multi-viewport workloads; report p50/p95/max, memory and click-to-selection latency against the design targets.
- [x] 9.3 Close editor, configure/build affected existing targets, rerun phase A regressions and native Vulkan dynamic/render-pass plus DX12 tests with custom providers and materials; keep untested platforms explicit.
- [x] 9.4 Review dependency/lifetime/error paths, update verification with actual phase B evidence and validate OpenSpec. Do not claim GPU implementation or phase B completion from the design alone; commit/archive still require confirmation.

## 10. Custom geometry query extension

- [x] 10.1 Define owned world-geometry entries, bounded resumable ray queries and cancellation/lifetime contracts in existing EditorCore provider snapshots; document scope without terrain/vegetation implementations.
- [x] 10.2 Compose custom bounds and exact hits with mesh/proxy resolution, filters and conservative incomplete states; validate foreign outputs and retain module leases through cursor destruction. Repair null/recovery provider handling on this path.
- [x] 10.3 Add an independent custom query fixture covering continuation, cancellation/revision/unregister, module lifetime, bounds/occlusion/ties/policies and invalid outputs; build affected targets and run Develop/Debug regressions.

## 11. Final review follow-up

- [x] 11.1 Align Overlay visual ordering and pick priority for overlapping proxies; promote the reproduction into EditorRenderTest and cover stable ties, reversed registration and locked occluders.
