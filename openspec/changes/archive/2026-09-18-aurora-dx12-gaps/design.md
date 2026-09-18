## Context

Aurora's D3D12 backend already has device, queues, swapchain, heaps, buffers/images, shaders, root signatures, resource groups, PSOs, RDG backend and command lists. What it lacks is the encode-time wiring that turns a compiled RDG pass into real GPU work:

- `D3D12GraphicsEncoder::BeginRendering` only sets viewport/scissor — it never calls `OMSetRenderTargets`, and `D3D12Image` has no `CreateRTV/CreateDSV`. No pass can write an attachment.
- `D3D12ComputeEncoder::BindResourceGroup` / `PushConstants` call `SetGraphicsRoot*`, which is invalid on a compute command list.
- Vertex input is empty: `PipelineState` has no vertex layout, DX12 PSO emits an empty `InputLayout`, and `BindVertexBuffers` hard-codes `StrideInBytes = 0`. The shared RDG executor never even binds `DrawItem::vb`.
- `BindIndexBuffer` hard-codes `SizeInBytes = 0`.
- `DrawIndirect` / `DrawIndexedIndirect` / `DispatchIndirect` are empty (D3D12 needs command signatures).
- `BlitImage` is empty (D3D12 has no `vkCmdBlitImage` equivalent).
- `D3D12Device::CreateDescriptorHeap` returns `nullptr`; there is no tier2 bindless path.

The goal is to bring D3D12 to the functional level of the Vulkan backend on the existing RDG + `ClientViewport` path. Per user decision this change uses the **input-assembler (IA)** vertex path (not vertex pulling) and **includes** the tier2 bindless descriptor heap.

## Goals / Non-Goals

**Goals:**
- Make a DX12 scene-raster / fullscreen pass produce correct pixels: attachments bound and cleared, draws issued.
- Fix the compute root-parameter bug.
- Real IA vertex/index binding with per-binding strides and correct index-buffer size.
- Real indirect draws and dispatch.
- A working `BlitImage` (copy path + scaled/filtered path).
- tier2 bindless `DescriptorHeap` on D3D12 with a Shader Model 6.6 capability gate and `BindDescriptorHeap`.

**Non-Goals:**
- Vulkan/Metal equivalent upgrades (they keep compiling; only shared struct additions are made).
- Descriptor-heap mapping sources beyond `HEAP_WITH_PUSH_INDEX` (`CONSTANT_OFFSET`, indirect index, etc.).
- Mesh shaders, VRS, sparse resources.
- Rewriting the RDG executor beyond adding vertex-buffer binding.

## Decisions

### D1. Vertex input layout lives on the shared `PipelineState` (IA path)

Add to `aurora/rhi/Core.h`:

- `VertexBindingDesc { binding, stride, inputRate }` already exists — keep.
- `VertexAttributeDesc { location, binding, offset, format, semantic, semanticIndex }` — rename the `sematic` typo to `semantic` and add `semanticIndex` (DX12 needs `SemanticName` + `SemanticIndex`).
- `PipelineState` gains `std::vector<VertexBindingDesc> vertexBindings;` and `std::vector<VertexAttributeDesc> vertexAttributes;`.

D3D12 PSO builds a real `D3D12_INPUT_LAYOUT_DESC` from the attributes and sets `InputLayout.NumElements`. `D3D12GraphicsEncoder::BindPipeline` caches `strides[binding]` from the PSO; `BindVertexBuffers` looks up `strides[firstBinding + i]` instead of hard-coding 0. The `BindVertexBuffers` interface is unchanged (strides are pipeline state, resolved through the currently bound PSO).

Rationale: matches the engine's existing `VertexBuffer::VertexLayout` metadata and keeps the RHI signature stable across backends. Alternative (vertex pulling / SRV) was rejected by the user.

The shared RDG executor (`Execute.cpp`, `SCENE_RASTER`) gains `enc->BindVertexBuffers(0, 1, &view)` for `DrawItem::vb` when non-null, before `BindIndexBuffer`/`DrawIndexed`. Vulkan/Metal then inherit IA binding for free.

### D2. RTV/DSV descriptors use a per-frame ring in `D3D12DescriptorAllocator`

Add two CPU-only descriptor heaps (`RTV`, `DSV`) sized per max attachments, with `ringSize` copies (one per in-flight frame) and a per-frame linear bump. `BeginFrame` resets the bump for the current frame; descriptors are not reused until that frame's slot comes around again, matching the existing CBV/SRV/UAV ring.

`D3D12Image` gains `CreateRTV(handle, ImageSubRange)` and `CreateDSV(handle, ImageSubRange)`. `BeginRendering` allocates `numColors` RTVs + 1 DSV, creates the views, calls `OMSetRenderTargets`, then clears color/depth/stencil according to `LoadOp::CLEAR` using `RenderingInfo::clearValue`.

Alternative: a single heap with deferred free — rejected as more bookkeeping for no benefit; pass-scoped bump fits the frame slot model.

### D3. Compute encoder uses compute-root variants

`D3D12ComputeEncoder::BindResourceGroup` and `PushConstants` switch to `SetComputeRootDescriptorTable` / `SetComputeRootConstantBufferView` / `SetComputeRootUnorderedAccessView` / `SetComputeRoot32BitConstants`. This is a straight correctness fix; the root signature already exposes the same param indices.

### D4. Index buffer size comes from a `Buffer` size accessor

Add `virtual uint64_t GetSize() const { return 0; }` to `aurora/rhi/Buffer.h`; `D3D12Buffer` overrides it and stores the descriptor size. `BindIndexBuffer` sets `SizeInBytes = GetSize() - offset` (clamped). `BufferView::range == 0` continues to mean "rest of buffer" for vertex views.

### D5. Indirect draws use cached command signatures

`D3D12Device` owns a small cache keyed by `(indirect kind, stride)` producing `ID3D12CommandSignature`:

- draw: argument = `D3D12_DRAW_ARGUMENTS` (4×u32), stride from caller.
- draw-indexed: `D3D12_DRAW_INDEXED_ARGUMENTS` (5×u32).
- dispatch: `D3D12_DISPATCH_ARGUMENTS` (3×u32).

`DrawIndirect` / `DrawIndexedIndirect` / `DispatchIndirect` then call `ExecuteIndirect` with `MaxCommandCount = drawCount` (or 1 for dispatch) and no counter buffer. Signatures are created lazily and live for the device lifetime.

### D6. `BlitImage` = copy path + built-in fullscreen blit pipeline

- When each region's `srcOffsets`/`dstOffsets` have equal extents and no filtering is requested, use `CopyTextureRegion` (fast path).
- Otherwise use a built-in fullscreen blit pipeline (`D3D12BlitHelper`) owning a root signature, a sampler, and a graphics PSO built from an **embedded precompiled DXIL** fullscreen blit shader (generated offline through the existing slang/DXC toolchain and checked in as a byte array). The helper renders src → dst as an RTV.

Rationale: keeps the RHI backend independent of the shader compiler module (layering: `aurora/rhi` must not depend on `aurora/shader`) while still supporting scaled/filtered blits. Alternatively only implement the copy path — rejected because it silently drops the filtering contract.

### D7. D3D12 tier2 descriptor heap

`D3D12DescriptorHeap : DescriptorHeap` owns two shader-visible heaps (resource = `CBV_SRV_UAV`, sampler). `Allocate` returns per-type index ranges from free lists (same free-range algorithm as `D3D12DescriptorAllocator`); `Update` writes through a `D3D12DescriptorEncoder` bound to the heap's CPU handles; `Free` returns ranges.

`D3D12Device::CreateDescriptorHeap` creates it only when `DeviceFeature::descriptorHeap` is true. `UpdateDeviceCaps` sets that flag from `CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL)` ≥ 6.6 (required for directly-indexed `ResourceDescriptorHeap`). `BindDescriptorHeap` on both encoders calls `SetDescriptorHeaps` with the heap pair.

Descriptor writes are immediate (D3D12 has no batched flush): the heap exposes `CreateHeapEncoder(allocation)` returning a heap-bound `DescriptorEncoder`; the base `Update(allocation, encoder)` is a no-op kept for interface symmetry (mirroring the ResourceGroup encoder on D3D12).

Mapping is `HEAP_WITH_PUSH_INDEX`: the root signature reserves a 32-bit root constant when descriptor-heap mapping is active, and the index is written per draw. **Deferred**: this push-index mapping depends on a shader/binding convention (where the per-draw index comes from on the slang side) that does not exist yet; it is split out as a follow-up. This change lands the heap object, allocation, immediate writes, `BindDescriptorHeap`, and the SM6.6 capability gate.

### D8. Doc/comment corrections

`engine/aurora/AGENTS.md` lines describing DX12 PSO as a stub and the `SlangD3D12Test.cpp` note are updated to reflect that PSO + root signature + resource groups exist.

## Risks / Trade-offs

- **Shared `PipelineState` change ripples to Vulkan/Metal.** → Additive fields with defaults; Vulkan currently ignores vertex bindings (empty input) and all backends still compile. Vulkan should consume the layout in a follow-up; not doing so leaves VK IA-path draws without strides, same as today.
- **`sematic`→`semantic` rename is source-breaking** for any user of `VertexAttributeDesc`. → Only the engine uses it today; grep before landing.
- **Embedded blit DXIL blob must be kept in sync with its source.** → Commit the HLSL source next to the generated header and document the regeneration command.
- **SM6.6 gate**: devices without SM6.6 return `nullptr` from `CreateDescriptorHeap` and callers must fall back to tier1. → Existing `DeviceFeature::descriptorHeap` default is `false`, matching the Vulkan fallback contract.
- **Descriptor heap correctness (aliasing/index collisions)** is easy to get wrong. → Reuse the proven free-range + per-frame isolation pattern and cover with tests.
- **Indirect path not exercised by any pass yet.** → Unit-test via `AuroraRHITest` with a manually filled argument buffer.

## Migration Plan

1. Land shared struct/interface additions first (`Core.h`, `Buffer.h`, `Execute.cpp`) so all backends compile.
2. Land DX12 encoder fixes (D2, D3, D4) — unblocks visible rendering.
3. Land IA wiring (D1) and indirect (D5), then blit (D6).
4. Land descriptor heap (D7) last; it is independently gated by capability.
5. Correct docs/comments (D8).

Rollback: each group is independently revertable; the shared additions are inert when unused.

## Open Questions

- Should Vulkan consume the new vertex-layout fields in this change or a follow-up? (Current plan: follow-up.)
- Is a scene-level test (SDL window + swapchain) required for DX12, or is a headless render-to-texture test enough for this change? (Current plan: headless `AuroraRHITest`; windowed smoke test manual.)
- Where should the embedded blit DXIL generation step live — `engine/aurora/shader/tool/` or a committed artifact with a documented manual command?
