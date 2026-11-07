> **Current status - 2026-11-07: review corrections implemented and verified on macOS; remaining work is listed below.** This change is a prerequisite for `add-particle-system`; it does not depend on particle code.

## 1. Extension contract

- [x] 1.1 Add `ISceneRenderExtension` and `SceneRenderStageContext` to the renderer feature contract
- [x] 1.2 Add `RegisterSceneExtension` / `RetireSceneExtension` commands with identity, duplicate and active-scene validation
- [x] 1.3 Extend the feature registry to retain extension instances until CPU/GPU borrowers retire
- [x] 1.4 Keep borrowed context, graph handles and view references non-retainable after callback return

## 2. Scene feature integration

- [x] 2.1 Create scene depth when at least one registered extension requires depth
- [x] 2.2 Invoke extensions after the opaque pass and before tonemap, sorted by order and name
- [x] 2.3 Pass the current view, scene color, scene depth and extent to each extension
- [x] 2.4 Enforce `LoadOp::LOAD` for shared color/depth and reject unsupported clear/attachment requests
- [x] 2.5 Propagate extension build failures and preserve the no-extension behavior

## 3. Tests and verification

- [x] 3.1 Add ordering tests with multiple extensions and stable name tie-breaks
- [ ] 3.2 Add color/depth sharing tests for HDR and non-HDR scene paths
- [x] 3.3 Add no-extension regression tests for existing scene output and pass order
- [x] 3.4 Add extension retirement/lifetime tests with in-flight frames
- [x] 3.5 Build affected Aurora libraries and tests; record unavailable platform checks explicitly
- [x] 3.6 Update verification evidence without commit or archive

## 4. Review corrections

- [x] 4.1 Unify extension registration/projection, sorted snapshots and independent retirement in FrameFeatureRegistry
- [x] 4.2 Route validated extension payloads with exact per-output resources and retain leases through GPU completion
- [x] 4.3 Cover capacity projection, stale handles, independent retirement and snapshot ownership with regression tests
- [x] 4.4 Replace the single-slot extension source with a multi-source registry (ordered append, identity-safe unregister, first-error propagation) and add registry tests
