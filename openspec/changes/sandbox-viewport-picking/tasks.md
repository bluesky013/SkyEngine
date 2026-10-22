Status: 提案已齐备，尚未实施。实现前先集成 `sandbox-viewport-gizmo-controls`；不要求提前归档该依赖。

## 1. Contracts and geometry

- [ ] 1.1 Add IViewportPicker and owned neutral query/result/status data in EditorCore; define context revisions and SelectionService application guards.
- [ ] 1.2 Implement ray/bounds/triangle queries with clipping, supported culling and world-distance sorting; test holes, overlaps, parent transforms, non-uniform/negative scales and degenerate/singular inputs.
- [ ] 1.3 Implement revision-keyed shared mesh BVH, bounded cache and recoverable preparation states; test reuse, eviction, reload and stale preparation results.

## 2. Existing adaptor integration

- [ ] 2.1 Verify MeshAssetData / SceneAssetSource / RenderSceneBridge ownership, visibility, layout, material culling and revision data; document the exact supported static mesh/material subset.
- [ ] 2.2 Expose owned neutral static mesh snapshots through the existing Aurora adaptor boundary, preserving Actor/part identity and geometry revision without Sandbox dependencies.
- [ ] 2.3 Implement the CPU picker in the existing EditorRender target and compose it in SandboxModule; avoid duplicate decoding, synchronous asset loads in input callbacks and new modules/targets.
- [ ] 2.4 Verify Pending/Unsupported/Error feedback and conservative nearest-hit completeness when relevant candidates are unresolved; keep unrelated unavailable assets from blocking valid queries.

## 3. Input and selection

- [ ] 3.1 Add click-candidate tracking to SceneViewport with themed maximum travel threshold, matching button/pointer and in-content release; preserve Gizmo/navigation capture priority.
- [ ] 3.2 Apply Hit/Miss through the existing SelectionService without World commands/dirty state; update Outliner, Inspector and Gizmo through their shared callbacks.
- [ ] 3.3 Cancel candidates and reject stale results on focus, modal/menu, tool, layout/DPI/surface, camera/scene revision, document, target and Play transitions.
- [ ] 3.4 Add main/floating Shell routing tests for supported selection, empty click, over-threshold return, competing gestures, pending completion, deleted targets and switched documents.

## 4. Verification and review

- [ ] 4.1 Close editor, configure/build affected existing targets including SandboxModule, then run core/shell/render/UI and relevant adaptor regressions.
- [ ] 4.2 Run native Vulkan and DX12 acceptance with overlapping meshes, bounds holes and a collider-free Actor; verify selection consumers, undo isolation and floating viewport coordinates.
- [ ] 4.3 Measure cold preparation, warm query latency and cache memory on recorded triangle/instance counts; record platform and material/deformation limitations separately from passing evidence.
- [ ] 4.4 Review module boundaries, error/lifetime paths and diff; validate OpenSpec and record actual results. Commit and archive remain subject to explicit user confirmation.
