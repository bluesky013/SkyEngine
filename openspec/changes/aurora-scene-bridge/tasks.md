# Scene bridge implementation batches

Status: active. Windows renderer B12/B14 prerequisites are satisfied; bridge implementation and Windows native integration are verified; the clang-tidy completion gate remains open. Keep these original task IDs for review history; renderer implementation order is maintained separately in [aurora-renderer/batches.md](../archive/2026-10-17-aurora-renderer/batches.md).

| Batch | Dependencies | Scope / task IDs | Exit criteria |
|---|---|---|---|
| S01 | Public IRenderer contract; archived Framework baseline | 1.1–1.3, 4.1–4.3, 5.4 | Fake renderer verifies owned packets, stable identity, Busy/Rejected and delayed retirement; no direct store dependency |
| S02 | S01 | 2.1–2.3, 3.1–3.3, 5.1–5.2 | Fake renderer receives correct mesh/light/camera, attach/detach and parented transforms; stale events cannot revive removed identities |
| S03 | S02; Windows renderer B12/B14 already satisfied | 5.1–5.4 production integration | Real provider applies the same commands; resources and shutdown obey renderer retirement; applicable adaptor/host builds pass |

Complete a checklist item only after all of its listed acceptance contexts pass. Every batch carries its implementation and this change's relevant specs together; commit/archive still require user confirmation.

## 1. Bridge skeleton

- [x] 1.1 Add `RenderSceneBridge` in `engine/aurora/adaptor` (links Framework + Aurora.Renderer.Interface).
- [x] 1.2 Define the per-frame sync point relative to `World::Tick` and the aurora scene build.
- [x] 1.3 Map World/Actor/component/part identity through a side table to generation-qualified renderer IDs; preserve tombstones until retirement acknowledgement.

## 2. Lifecycle subscriptions

- [x] 2.1 Subscribe to actor/component attach and detach to create/remove scene entries.
- [x] 2.2 Subscribe to `TransformComponent` changes to update world transforms.
- [x] 2.3 Publish logical resource references and consume readiness/error events without blocking or borrowing asset pointers.

## 3. First component coverage

- [x] 3.1 Static mesh component data.
- [x] 3.2 Light component data.
- [x] 3.3 Camera component data.

## 4. One-way and render-free enforcement

- [x] 4.1 Bridge submits owned FramePackets; only the renderer coordinator writes ECS (no write-back or direct ECS access).
- [x] 4.2 Bridged component persisted data includes no aurora/render implementation types.
- [x] 4.3 Handle Busy/Rejected ownership and bounded retry; reuse IDs only after retirement acknowledgement.

## 5. Verification

- [x] 5.1 Test: attach/detach creates/removes the ECS entry.
- [x] 5.2 Test: a parented actor's updated world transform reaches the bridge.
- [ ] 5.3 Build aurora adaptor targets; clang-format/clang-tidy.
- [x] 5.4 Test: queued data survives source destruction, Busy preserves the packet, and stale generation updates cannot revive a removed entity.

2026-10-21 verification: Aurora.AdaptorTest 29/29, Aurora.PipelineTest 32/32, SandboxProjectTest 9/9; GPU suite 870 passed / 3 backend-specific skips. Native editor create/edit/undo/save/reopen and World retirement succeeded. Revisions are immutable for a source lifetime; hot reload is not claimed. Windows does not validate Metal/mobile.

Final follow-up: Aurora.AdaptorTest 32/32, Aurora.PipelineTest 33/33 and SandboxProjectTest 10/10 passed. Changed C++ formatting passed. The seven scoped clang-tidy runs completed with exit code 0, but still report complexity, missed moves and conservative optional/string-view/allocation diagnostics; 5.3 remains open for that cleanup rather than claiming a warning-free result. The optional access is guarded by the preceding has_value-derived flag; the string-view warning concerns a compile-time literal asset type. No exception handling was introduced to suppress allocation diagnostics.
