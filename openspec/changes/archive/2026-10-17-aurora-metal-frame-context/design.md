## Context

The new Aurora renderer drives backends through `DeviceFrameContext`: begin an explicit slot, compile a frozen graph into an immutable plan, dispatch/collect encoding, submit, and poll completion. Vulkan and DX12 implement this. Metal has:

- an empty `MetalDeviceFrameContext` that only inherits `BackendFrameContext`;
- explicit `API::METAL` rejection in `TryBeginSlot`, tracking and `CanChangeTracking`;
- no `FrameTransientPool` on `MetalDevice`;
- no `TryReset` override on `MetalCommandPool` or `MetalFence`;
- no Metal compile/dispatch implementation.

## Goals / Non-Goals

**Goals:**

- Prove Metal can use the same slot/lease/submit/completion protocol.
- Support offscreen clear/load/store plus buffer/image copies and readback.
- Keep every unsupported operation explicit.
- Add a runnable macOS test with numeric readback.

**Non-Goals:**

- No draws, compute, uploads, native-window output, present, resize/close or MSAA/depth resolve in this milestone.
- No legacy host deletion.
- No changes to the public `DeviceFrameContext` API.
- No Windows/Linux behavior change.

## Decisions

### 1. Remove the backend-name rejection, keep capability checks

The `API::METAL` early returns in `BackendFrameContext` are temporary migration guards, not protocol requirements. Remove them from `TryBeginSlot`, `CanChangeTracking`, `TrackResource`, `GetResourceState` and `UntrackResource`. Metal-specific unsupported operations remain implemented as explicit errors in the Metal compile path.

### 2. Add the missing sync/pool primitives

`MetalFence` implements `Query` and `TryReset` using its shared-event value; `MetalCommandPool::TryReset` recycles wrapper command buffers after the common layer has confirmed the slot retired. These are required for the common submission and completion paths.

### 3. Reuse `FrameTransientPool`

`MetalDevice` owns a `FrameTransientPool` like Vulkan/DX12, so graph transient resources are allocated through the same epoch/lease policy.

### 4. Implement a minimal Metal graph compiler

The first compiler supports:
- `BufferCopyPass` and `BufferImageCopyPass` via `MetalBlitEncoder`;
- `FrameRasterPass` with one color attachment and draws, using `MetalGraphicsEncoder::BeginRendering`, per-draw `GraphicsPipeline`, `ResourceGroup` bindings, vertex/index buffers and draw calls.

Color local read and unsupported view forms return `UNSUPPORTED`; depth resolve is implemented through a depth store-action resolve pass. `FrameComputePass` / `FrameCustomPass` remain explicit `UNSUPPORTED` placeholders until their resource declarations and encoders are added. The plan owns the frozen graph, resolved bindings, and a small vector of steps; dispatch records the steps into one command buffer and publishes the recording synchronously. Worker partitioning and parallel fragment encoding are deferred.

### 5. Test from the RHI layer

Add a Metal-only test under `engine/aurora/rhi/test` using `AuroraMetalTest` and direct `FrameGraphData` construction, so the test does not depend on the shader/pipeline path yet. Build it on macOS and run it in `ctest`.

## Risks / Trade-offs

- **[Clear-only is a narrow milestone]** → It validates slot/fence/submit/readback, which are the highest-risk integration points; draws follow separately.
- **[Metal resource views for mip/layer]** → This milestone uses the base texture only; mip/layer view support is required with draws.
- **[Transient pool ownership]** → Metal transient resources are created through `Device::CreateImage/CreateBuffer`; validate device ownership before recording.
- **[Fence reset semantics]** → `TryReset` advances the shared-event target; submit advances again. Verify completion polling and reuse across two slots.
- **[Legacy host remains]** → Intentionally out of scope; `aurora-renderer-platform-parity` still owns removal.

## Migration Plan

1. Add fence/pool/transient-pool primitives.
2. Remove Metal early rejection from the common slot/tracking path.
3. Implement compile/dispatch for clear/copy.
4. Add and run the Metal offscreen test.
5. Record results; later milestones add draws/compute/upload/window/host unification.

Rollback is a straight revert; no public API or data format changes.

## Open Questions

None for this milestone. Draw/compute and native-window acceptance remain in `aurora-renderer-platform-parity`.
