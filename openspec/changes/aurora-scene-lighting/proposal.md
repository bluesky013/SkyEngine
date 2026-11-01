> 2026-11-01 整理：**implemented-awaiting-platform-validation**。现行 owner：功能里程碑保留于本 change；现行原生/视觉验收归 `aurora-renderer-platform-parity` P02-LIGHTING。
> 剩余：5.2 仍未验收；Metal 深度和手动画面只覆盖子集，不替代 HDR/IBL/移动端完整像素矩阵。下方较早状态与验收记录保留为历史。

## Why

World objects now reach the shared renderer, but the default scene pass only applies a tint. The approved next stages require scene lighting, shadow resources and environment inputs to use the same pass-owned shared declarations as ordinary draws.

## What Changes

- Add validated depth sampling and depth-only raster attachments, followed by HDR color targets, to the existing frozen graph and backend compilers.
- Add a bounded forward-lighting scene pass and a directional shadow pass, including casters outside the camera frustum.
- Add explicit environment input bindings, diffuse/specular IBL and a final tone mapping pass with neutral defaults when inputs are absent.
- Preserve Global set 0 / Pass set 1 (View optional) / Material set 2 / Batch set 3. Pass declarations are authoritative `.h` sources consumed by C++; shaders consume persisted `.slangh` exports and target ABI is encoded through UniformUploadPlan.
- Verify graph rejection paths and actual pixels independently in Vulkan dynamic rendering, Vulkan render passes and DX12. Metal implementation and unexecuted native validation are recorded separately.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-pipeline`: Pass-owned light, shadow and environment inputs with explicit defaults, bounded work and native pixel verification.

This capability specifies the additive scene pipeline and its required frozen-graph subset.

## Impact

Existing Aurora pipeline, core preparation, adaptor components, shader generation and RHI compilers are affected. There are no new module targets, ownership changes or directory moves. PreparedRaster retains only geometry/exact-version resolution; the editor still has one packet producer and event consumer. Implementation and this change are reviewed together; archive and commit require user confirmation.
