> 2026-11-01 整理：**implemented-with-quality-debt**。现行 owner：本 change 维护单向 World→owned packet bridge；5.3 仅保留明确的 clang-tidy 清理债务。
> 剩余：构建、格式与功能回归已完成；5.3 的警告清理尚未关闭，不伪称 warning-free。下方较早状态与验收记录保留为历史。

## Why

原阶段状态：active（可实施）。Windows Renderer B12/B14 能力门已通过，S01/S02/S03 不再等待 Renderer 主线；非 Windows 集成需等 aurora-renderer-platform-parity。

Framework components are the scene authoring model, while aurora renders from a data-oriented scene ECS. Today the two coexist without a defined transfer path. This is an addition to the existing **aurora adaptor** capability (not a new capability): the adaptor already bridges components/assets; this adds the one-way scene→ECS hand-off.

## What Changes

- Add a `RenderSceneBridge` that batches lifecycle and transform values into owned FramePackets through IRenderer; the render coordinator alone applies them to ECS.
- Keep the transfer strictly one-way: framework → aurora; render state lives only in aurora.
- Keep components render-free at the persistence layer: the bridge reads only `Uuid` + POD and the transform world matrix.
- Scope to the Aurora path; the legacy renderer has been removed.

## Capabilities

### Modified Capabilities

- `aurora-renderer`: adds one-way scene-to-ECS hand-off (lifecycle-driven create/remove, transform transfer) alongside the existing component/asset bridge.

## Impact

- Code: new bridge in `engine/aurora/adaptor`, consuming Framework events and `Aurora.Renderer.Interface`; no direct SceneStore/ECS dependency.
- Uses the archived [harden-framework-components baseline](../archive/2026-10-06-harden-framework-components/proposal.md) for deterministic lifecycle order and correct transform propagation; retain its regression coverage.
- Consumes the packet, identity, resource-version and retirement contracts of [aurora-renderer](../archive/2026-10-17-aurora-renderer/proposal.md). Contract tests can use a fake IRenderer before the production provider is ready.
- No change to framework public component types or aurora RHI.

## Delivery

Keep this change separate from renderer internals. [Tasks](tasks.md) are grouped into S01 (owned command/identity contract), S02 (lifecycle, transforms and component extraction), and S03 (production integration). The Windows prerequisites for S03 (renderer B12 and B14) are satisfied; S01/S02 remain the implementation prerequisites. Batch definitions and platform gates for the renderer remain in its [implementation plan](../archive/2026-10-17-aurora-renderer/batches.md).

This change owns host extraction only. It does not redefine Completion, direct ECS writes, upload readiness or GPU/presentation retirement. Existing task IDs remain stable; this documentation reorganization does not mark implementation complete.
