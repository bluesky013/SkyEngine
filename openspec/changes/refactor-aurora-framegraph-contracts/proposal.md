> 2026-11-01 整理：**active**。现行 owner：本 change 保持最终公共参数/DirectFrame/绑定组契约；平台矩阵由 `aurora-renderer-platform-parity` 接续。
> 2026-11-11 续作：本 change 同时承接已确认的后端编译归属、显式 output Export、分析预算及局部精简；2026-11-12 完成本机验证。Windows 原生回归 4.5 与 P02-LIGHTING 5.4 仍未验收，历史验证不代替本轮证据。

## Why

FrameGraph binding, content analysis and native synchronization independently enumerate resource uses, which has left raster storage reads unsynchronized on Vulkan/DX12. Metal also separates resolve from attachment preservation, and the public binding/context interfaces mix incompatible update and execution protocols. The approved review calls for explicit ownership and reusable immutable binding groups while preserving backend-specific compilation.

## What Changes

- Derive compile-local resource-use facts once, preserving reads, writes, defines, effective ranges and native-independent usage roles.
- Fix raster storage synchronization, Metal attachment resolve/preservation, and native plan identity/single-consumption handling.
- Freeze logical binding groups and reuse target encoding/upload and native materialization within their graph/lease; keep dynamic offsets in draw state.
- **BREAKING** Separate named immutable parameter updates from numeric native descriptor writes, and isolate the legacy direct-frame protocol from the explicit lease context.
- Expose the existing supported graph declaration operations independently of feature resource/draw resolution services.
- Add focused semantic, allocation/reuse, lifecycle and native regressions; record unavailable platform checks explicitly.
- Preserve device-level Vulkan dynamic-rendering preference and physical Vulkan 1.1/KHR capability negotiation; contexts consume the selected backend path. The earlier public AUTO-mode API is historical and has been removed.
- Complete the approved 2026-11-11 follow-up: explicit output exports, selected-technique readiness, pre-expansion analysis budgets, backend-owned Metal compiler/encoder, bounded device PSO caches and local composition simplification.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `aurora-renderer`: Explicit per-frame output export requests, selected-technique resource readiness and frame-local draw resolution.

- `aurora-framegraph`: Compile-local access facts, immutable logical groups, complete declaration services, explicit parameter/direct protocols and backend lifecycle parity.
  Freeze immutable group references with per-use overrides, unify pre-cull validation/resource uses, and preserve native synchronization/resolve/plan ownership.

- `aurora-shaders`: Separate numeric native DescriptorEncoder from named immutable ShaderParameterUpdate/CreateParameterUpdate; update direct compatibility descriptor-ring ownership.
  Bind the Android Vulkan SPIR-V 1.3 baseline to actual code, target environment and producer identity; recook incompatible earlier products.

- `aurora-rhi`: Remove DescriptorBatch, split explicit lease and separately-created DirectFrameContext, and preserve checked allocation/retirement boundaries. The historical `aurora-frame-context` capability was consolidated into this main spec; no duplicate capability is introduced.

## Impact

Aurora RHI interfaces/common/native backends, renderer graph composition, Pipeline consumers and associated tests. No target, module ownership, loading entry or directory changes. The existing `refactor-aurora-framegraph-internals` extraction remains valid; this change owns the subsequent semantic/API work. Existing shared pass declarations and shader artifacts remain authoritative. No commit or archive is authorized by implementation approval.

## Contract Ordering

This change owns the final public parameter/direct/group contracts. Its modified and removed requirements supersede the older named-DescriptorEncoder wording in `refactor-aurora-target-shader-artifacts`, the pre-group declaration wording in `aurora-compute-custom-passes`, and any conflicting direct/batch wording in the current main specs. Their target reflection, shared declaration, content coverage, feature support and lease-retirement semantics remain authoritative where unchanged. Existing active change records remain intact; any eventual spec synchronization must apply these successor deltas after their older overlapping wording. This is not authorization to synchronize or archive changes.
