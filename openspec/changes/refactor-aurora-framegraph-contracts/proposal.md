## Why

FrameGraph binding, content analysis and native synchronization independently enumerate resource uses, which has left raster storage reads unsynchronized on Vulkan/DX12. Metal also separates resolve from attachment preservation, and the public binding/context interfaces mix incompatible update and execution protocols. The approved review calls for explicit ownership and reusable immutable binding groups while preserving backend-specific compilation.

## What Changes

- Derive compile-local resource-use facts once, preserving reads, writes, defines, effective ranges and native-independent usage roles.
- Fix raster storage synchronization, Metal attachment resolve/preservation, and native plan identity/single-consumption handling.
- Freeze logical binding groups and reuse target encoding/upload and native materialization within their graph/lease; keep dynamic offsets in draw state.
- **BREAKING** Separate named immutable parameter updates from numeric native descriptor writes, and isolate the legacy direct-frame protocol from the explicit lease context.
- Expose the existing supported graph declaration operations independently of feature resource/draw resolution services.
- Add focused semantic, allocation/reuse, lifecycle and native regressions; record unavailable platform checks explicitly.

## Capabilities

### New Capabilities
- `framegraph-contracts`: Compile-local access facts, immutable logical groups, complete declaration services, explicit parameter/direct protocols and backend lifecycle parity.

### Modified Capabilities

- `aurora-resource-binding`: Separate numeric native DescriptorEncoder from named immutable ShaderParameterUpdate/CreateParameterUpdate; update direct compatibility descriptor-ring ownership.
- `aurora-rhi-core`: Remove DescriptorBatch, split explicit lease and separately-created DirectFrameContext, and preserve checked allocation/retirement boundaries. The historical `aurora-frame-context` capability was consolidated into this main spec; no duplicate capability is introduced.
- `aurora-rdg`: Freeze immutable group references with per-use overrides, unify pre-cull validation/resource uses, and preserve native synchronization/resolve/plan ownership.

## Impact

Aurora RHI interfaces/common/native backends, renderer graph composition, Pipeline consumers and associated tests. No target, module ownership, loading entry or directory changes. The existing `refactor-aurora-framegraph-internals` extraction remains valid; this change owns the subsequent semantic/API work. Existing shared pass declarations and shader artifacts remain authoritative. No commit or archive is authorized by implementation approval.

## Contract Ordering

This change owns the final public parameter/direct/group contracts. Its modified and removed requirements supersede the older named-DescriptorEncoder wording in `refactor-aurora-target-shader-artifacts`, the pre-group declaration wording in `aurora-compute-custom-passes`, and any conflicting direct/batch wording in the current main specs. Their target reflection, shared declaration, content coverage, feature support and lease-retirement semantics remain authoritative where unchanged. Existing active change records remain intact; any eventual spec synchronization must apply these successor deltas after their older overlapping wording. This is not authorization to synchronize or archive changes.
