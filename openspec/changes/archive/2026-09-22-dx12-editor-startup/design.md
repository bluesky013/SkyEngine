## Context

The sandbox editor drives Aurora through `EditorRenderer` (device + frame context + `ClientViewport` + UI). The
checked DX12 startup produced:

```
[AuroraDX12] D3D12 device created successfully
[D3D12PipelineState] failed to create graphics pipeline state, hr=0x80004005   (E_FAIL)
[UIRenderer] UI pipeline creation failed
[AuroraDX12] buffer Map failed: 0x887a0005                                      (DXGI_ERROR_DEVICE_REMOVED)
```

Before that the process crashed with no output, and DX12 could not be selected at all.

## Gap assessment

- **G1 - No backend query.** `Instance::Init` maps `API::DEFAULT` to `AuroraVulkan`; `Device` exposes no API
  accessor. A shader consumer cannot know whether to emit SPIR-V or DXIL.
- **G2 - UI shader target hardcoded to SPIR-V.** `UIRenderer::Init` (`engine/ui/render/src/UIRenderer.cpp:102`)
  always uses `ShaderTarget::SPIRV`. DX12 needs DXIL (`ShaderCompilerSlang` supports `SLANG_DXIL`, profile
  `sm_6_5`), so `CreateGraphicsPipelineState` returns `E_FAIL` and the device is removed.
- **G3 - Debug-layer break is fatal.** `D3D12Device::CreateDevice` calls `SetBreakOnSeverity(ERROR, TRUE)` in debug
  builds. Without an attached debugger a single error-severity message terminates the process (buffered `printf`
  logs are lost, so it looked like an instant silent crash).
- **G4 - DX12 input layout semantics mismatch (found).** With DXIL in place the PSO still failed with
  `E_INVALIDARG`. The D3D12 backend names input elements from `VertexAttributeDesc::semantic`, but `UIRenderer`
  left it null, so all attributes became `TEXCOORD0` while the shader declares `POSITION`/`TEXCOORD0`/`COLOR0`.
  Fixed by setting the UI vertex semantics.
- **G5 - Root signature sampler register mismatch (found).** With semantics fixed, the debug layer reported
  `Root Signature doesn't match Pixel Shader: Shader sampler descriptor range (BaseShaderRegister=0, ...) is not
  fully bound`. The UI used a **hand-written** Vulkan-style reflection (sampler binding 1) but DXIL assigns the
  sampler to `s0`. Fixed by using the compiler's target-specific reflection (`ShaderCompileResult::reflection`).
- **G6 - Root signature never bound on DX12 (found, fixed).** After G4/G5 the device/pipeline/viewport/swapchain
  all initialize, but the UI draw path crashed inside `UIRenderer::Render`. `D3D12GraphicsEncoder::BindPipeline`
  set the pipeline state but never called `SetGraphicsRootSignature` (only the blit helper did), so the first
  `SetGraphicsRootDescriptorTable` hit an unbound root signature -> debug-layer corruption -> `0x87A` terminate.
  Fixed by binding the root signature in the graphics **and** compute `BindPipeline`.
- **G7 - Primitive topology never set (found, fixed).** The DX12 debug layer reported
  `DrawIndexedInstanced: ... primitive topology ... D3D_PRIMITIVE_TOPOLOGY_UNDEFINED`. Topology is command-list
  state on DX12 and the RHI has no topology setter, so the encoder never called `IASetPrimitiveTopology`. Fixed by
  mapping `PipelineState::inputAssembly.topology` and setting it in `BindPipeline`.
- **G8 - Shared command allocator reset (found, fixed; device removal).** The debug layer reported
  `ExecuteCommandLists: The command allocator was reset after the command list was recorded`, then
  `RemoveDevice ... DXGI_ERROR_INVALID_CALL`. `D3D12CommandPool` shared one allocator across all command buffers
  and each `Begin()` reset it, so recording the preview buffer invalidated the already-recorded main buffer. Fixed
  by giving each command buffer a dedicated allocator. The `ResourceBarrier` before-state errors seen alongside
  were a consequence of the removed device and cleared once the allocator was fixed.
- **G9 - UI upside down on DX12 (found, fixed).** With DX12 running, the editor UI rendered vertically flipped.
  `UIRenderer::UpdateDrawData` hardcoded `y_clip = 2y/h - 1`, which is correct only for Vulkan's **Y-down** NDC;
  D3D12/Metal NDC is **Y-up**, so the same mapping inverted the image. Fixed by exposing the backend clip-space Y
  axis on the device and choosing the projection sign from it.

## Goals / Non-Goals

**Goals:**
- Let the sandbox editor select the RHI backend at startup (`--rhi`, default Vulkan).
- Make the UI shader target follow the active backend, so the UI pipeline can be created on DX12.
- Stop the DX12 debug layer from terminating the process when no debugger is attached.
- Drive the startup path to completion on DX12, fixing the gaps reached along the way.

**Non-Goals:**
- Full DX12 rendering parity (scene pipeline, RDG passes) - this change only covers the editor startup/UI path.
- Metal validation (the change keeps the target mapping consistent but does not validate Metal).
- Offline shader tooling (`ShaderHeaderTool` keeps using SPIR-V for reflection).

## Decisions

- **D1 - Report the API from the device, not the descriptor.** Add `Device::GetAPI()` (implemented by each backend
  from its own definition of the active API). Rationale: consumers hold a `Device*`, not the `Instance::Descriptor`.
  Alternative considered: store the API on `Instance` and pass it down - rejected, it widens signatures everywhere.
- **D2 - Map the active API to a `ShaderTarget`.** Vulkan -> SPIRV, DX12 -> DXIL, Metal -> MSL. `UIRenderer::Init`
  uses the mapping from `device->GetAPI()`. Keep it in one helper so future shader consumers share it.
- **D3 - Break only when a debugger is attached.** `SetBreakOnSeverity(..., IsDebuggerPresent() ? TRUE : FALSE)`.
  Rationale: developers with a debugger keep the break-on-error behavior; CI/headless runs and manual launches do
  not die on a non-fatal debug-layer message. Follow-up (not here): forward info-queue messages to the logger.
- **D4 - Select the backend in the editor host, pass `API` into the renderer.** `SandboxModule` parses `--rhi` and
  `EditorRenderer::Init` takes an `API` (default `API::DEFAULT`). Mirrors `AuroraModule::ProcessArgs`.
- **D5 - The UI reflection comes from the compiler, not hand-written.** The shader compiler already returns a
  target-specific `ShaderReflection` (Vulkan bindings vs DX12 register numbers). Using it keeps the root signature
  consistent with the shader on every backend; a hand-written reflection cannot be correct for both.
- **D6 - Vertex attributes must carry DX12-compatible semantics.** The UI provides `POSITION`/`TEXCOORD`/`COLOR`
  names so the D3D12 input layout matches the shader input signature (Vulkan matches by location and ignores
  semantics, which is why this only surfaced on DX12).
- **D7 - `BindPipeline` binds the root signature.** The root signature is command-list state; the DX12 graphics and
  compute encoders set it in `BindPipeline` so later resource-group writes are valid. This was a latent DX12 bug
  that also affected the non-UI path, not just the editor.
- **D8 - Primitive topology is command-list state.** `D3D12GraphicsEncoder::BindPipeline` calls
  `IASetPrimitiveTopology(FromPrimitiveTopologyValue(state.inputAssembly.topology))` (a `D3D_PRIMITIVE_TOPOLOGY`,
  distinct from the PSO's topology type).
- **D9 - One command allocator per command buffer.** `D3D12CommandPool::Allocate` creates a dedicated
  `ID3D12CommandAllocator` for each buffer, so several buffers from one pool can be recorded in the same frame
  without resetting each other's allocator (main + preview).
- **D10 - The projection's Y sign follows the device clip-space axis.** `DeviceCapability::clipSpaceYDown` (Vulkan
  true; D3D12/Metal false, filled in `UpdateDeviceCaps`) drives the UI ortho projection's Y sign, so pixel y=0
  lands at the framebuffer top on every backend. General projection builders authored for a Y-up convention can
  use the same flag to decide whether to flip.

## Risks / Trade-offs

- [DX12 UI path may have more gaps] → fix incrementally, keeping Vulkan as the default so the editor still works.
- [New `Device::GetAPI()` is a pure virtual] → implement in all three backends in the same change to avoid
  breaking the build.
- [DXIL runtime compilation cost] → acceptable for the editor UI shader (one small shader); caching is out of scope.

## Migration Plan

1. Add `Device::GetAPI()` (interface + Vulkan/DX12/Metal).
2. Guard the DX12 debug-layer break with `IsDebuggerPresent()`.
3. Add backend-aware `ShaderTarget` selection to `UIRenderer::Init`.
4. Parse `--rhi` in `SandboxModule` and pass the `API` through `EditorRenderer::Init`.
5. Run the editor with `--rhi dx12`; fix the next gap reached (swapchain/UI upload); re-verify Vulkan still works.
6. Rollback: revert the four file groups; Vulkan default behavior is unchanged.
