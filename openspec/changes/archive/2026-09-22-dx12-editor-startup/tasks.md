## 1. Backend query

- [x] 1.1 Add `Device::GetAPI()` to the RHI interface
- [x] 1.2 Implement `GetAPI()` in the Vulkan, DX12 and Metal backends

## 2. Safe debug layer

- [x] 2.1 Guard the DX12 break-on-severity with `IsDebuggerPresent()`

## 3. Backend-aware shader / pipeline

- [x] 3.1 Add an API -> `ShaderTarget` mapping (Vulkan=SPIRV, DX12=DXIL, Metal=MSL)
- [x] 3.2 Use the mapping in `UIRenderer::Init` instead of hardcoded SPIRV
- [x] 3.3 Give the UI vertex attributes proper semantics (`POSITION` / `TEXCOORD` / `COLOR`) so the DX12 input
      layout matches the shader signature
- [x] 3.4 Use the compiler's target-specific reflection instead of a hand-written one, so the DX12 root signature
      sampler register matches the shader
- [x] 3.5 Bind the root signature in DX12 `BindPipeline` (graphics + compute): it was never set, so any
      `SetGraphicsRootDescriptorTable` hit an unbound root signature
- [x] 3.6 Set the primitive topology in DX12 `BindPipeline` (`IASetPrimitiveTopology`); it was never set, so
      `DrawIndexedInstanced` ran with `D3D_PRIMITIVE_TOPOLOGY_UNDEFINED`
- [x] 3.7 Give each DX12 command buffer a dedicated command allocator (was one shared per pool, so recording the
      preview buffer reset the already-recorded main buffer and removed the device)
- [x] 3.8 Add `DeviceCapability::clipSpaceYDown` (Vulkan true; DX12/Metal false) and pick the UI ortho
      projection's Y sign from it, so the editor UI is upright on every backend

## 4. Editor backend selection

- [x] 4.1 Parse `--rhi <vulkan|dx12|metal>` in `SandboxModule`
- [x] 4.2 Pass the `API` through `EditorRenderer::Init`

## 5. Verify

- [x] 5.1 Reach the DX12 startup path: device + UI pipeline now succeed (PSO no longer `E_FAIL` / `E_INVALIDARG`)
- [x] 5.2 Confirm `SandboxEditor` (default Vulkan) still runs unchanged (exit 0, no errors)
- [x] 5.3 Run the editor on DX12: first frame records and executes the UI draws (exit 0)
- [x] 5.4 Run sustained DX12 frames with no debug-layer errors (180 frames, exit 0, 0 errors) and no device removal

## 6. Assess / record

- [x] 6.1 Record the DX12 gaps found (see design.md "Gap assessment" G1-G8)
- [x] 6.2 Fix the DX12 device-removal (`DXGI_ERROR_INVALID_CALL`): primitive topology + per-buffer allocator
