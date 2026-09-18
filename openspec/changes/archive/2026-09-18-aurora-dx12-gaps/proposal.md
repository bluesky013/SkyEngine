## Why

The new Aurora RHI (`dev_refactor_rhi`) can create D3D12 devices, resources, shaders, root signatures, resource groups, swap chains and command buffers, but the D3D12 **encoder path is not yet able to actually render**: `BeginRendering` never calls `OMSetRenderTargets`, so nothing ever reaches a color or depth attachment. On top of that the compute encoder binds graphics-root parameters, vertex input has no stride/input layout, indirect draws and `DispatchIndirect` are empty, `BlitImage` is empty, and the tier2 bindless `DescriptorHeap` returns `nullptr`. Vulkan is the only backend that can currently produce a real frame end-to-end.

This change closes the D3D12 gaps so the backend reaches the same functional level as Vulkan and can be validated on the existing RDG + `ClientViewport` path.

## What Changes

- **Render target binding + clear**: `D3D12GraphicsEncoder::BeginRendering` creates RTV/DSV views for the pass attachments, calls `OMSetRenderTargets`, and clears color/depth per `LoadOp`. Adds `D3D12Image::CreateRTV/CreateDSV` and per-frame RTV/DSV descriptor rings.
- **Compute root binding fix**: `D3D12ComputeEncoder` uses `SetComputeRootDescriptorTable` / `SetComputeRootConstantBufferView` / `SetComputeRootUnorderedAccessView` / `SetComputeRoot32BitConstants` instead of the graphics variants.
- **Input assembly (IA)**: `PipelineState` carries vertex bindings + attributes; DX12 PSO builds a real `D3D12_INPUT_LAYOUT_DESC`; the encoder caches per-binding strides from the bound PSO and applies them in `BindVertexBuffers`; the RDG scene-raster executor binds `DrawItem::vb`.
- **Index buffer size**: `D3D12Buffer` exposes its size and `BindIndexBuffer` derives `SizeInBytes` from the buffer/offset instead of hard-coding 0.
- **Indirect draws**: persistent `ID3D12CommandSignature` objects (draw / draw-indexed / dispatch) and real `ExecuteIndirect` for `DrawIndirect`, `DrawIndexedIndirect` and `DispatchIndirect`.
- **`BlitImage`**: same-extent copies via `CopyTextureRegion`; scaled/filtered blits via a built-in fullscreen blit pipeline with an embedded DXIL blob.
- **tier2 bindless descriptor heap**: `D3D12DescriptorHeap : DescriptorHeap` (resource + sampler backing heaps, per-type index allocation, heap-bound descriptor encoder), `D3D12Device::CreateDescriptorHeap`, `BindDescriptorHeap` on graphics/compute encoders, and an SM6.6 capability gate. The `HEAP_WITH_PUSH_INDEX` per-draw index mapping is deferred to a follow-up (needs a shader/binding convention).
- Update the stale "PSO is a stub" notes in `engine/aurora/AGENTS.md` and `SlangD3D12Test.cpp`.

## Capabilities

### New Capabilities
- `aurora-dx12-rendering`: D3D12 encoder completeness — render-target binding/clear, input assembly, index buffer sizing, indirect draws/dispatch, and image blit.

### Modified Capabilities
- `aurora-resource-binding`: add the D3D12 tier2 bindless `DescriptorHeap` (backing heaps, per-type index allocation, encoder `Update`, `BindDescriptorHeap`) and the Shader Model 6.6 capability gate alongside the existing Vulkan descriptor-heap requirements.
- `aurora-rhi-core`: pipeline state owns vertex input layout (bindings + attributes); `Buffer` gains a size accessor used for index-buffer binding.
- `aurora-rdg`: the shared scene-raster executor binds vertex buffers from `DrawItem` before indexed draws.

## Impact

- Affected backend code: `engine/aurora/rhi/dx12/` (`D3D12Encoder`, `D3D12PipelineState`, `D3D12Image`, `D3D12Buffer`, `D3D12Device`, `D3D12DescriptorAllocator`, new `D3D12DescriptorHeap`), plus new command-signature and blit helpers.
- Shared interfaces touched: `aurora/rhi/Core.h` (`PipelineState`), `aurora/rhi/Buffer.h` (size accessor), `aurora/rdg` executor (`Execute.cpp`). Vulkan/Metal consume the same new pipeline fields and must keep compiling (Vulkan already has an empty vertex-input PSO; wiring it is a follow-up unless trivial).
- No public engine API break: additions are additive; `BindVertexBuffers` signature is unchanged (strides come from the bound PSO).
- Tests: extend `AuroraRHITest` (DX12 render-target/IA/indirect/blit/heap cases), keep `AuroraShaderTest` DX12 green.
- Docs: correct stale AGENTS.md / test comments describing DX12 PSO as a stub.
