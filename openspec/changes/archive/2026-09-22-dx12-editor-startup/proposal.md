## Why

The sandbox editor cannot start on the DX12 backend. Two independent blockers were found while checking it:

1. **The editor cannot even select DX12.** `EditorRenderer` hardcodes `API::DEFAULT`, and `Instance::Init` maps
   `DEFAULT` to `AuroraVulkan`; the editor's start-argument parser handles only `--frames`.
2. **With DX12 selected the process dies.** In debug builds the DX12 backend calls
   `SetBreakOnSeverity(ERROR, TRUE)`, so any error-severity DX12 message triggers `DebugBreak()` — which terminates
   the process when no debugger is attached. Once that is guarded, the real failure shows: the UI pipeline state
   creation fails with `E_FAIL` because `UIRenderer::Init` always compiles the UI shader to **SPIR-V**, while DX12
   requires **DXIL**.

Aurora has no way for a consumer to learn which backend is active, so the UI shader target cannot follow the
device.

## What Changes

- **Expose the active backend**: the Aurora `Device` SHALL report the active `API`, so shader-consuming code can
  pick the right compilation target.
- **Backend-aware UI shader target**: `UIRenderer::Init` SHALL compile the UI shader to the target matching the
  active backend (DXIL for DX12, SPIR-V for Vulkan, MSL for Metal) instead of always SPIR-V.
- **Safe debug-layer policy**: the DX12 debug layer SHALL only break on error/corruption when a debugger is
  attached; otherwise the message is reported without terminating the process.
- **Editor backend selection**: the sandbox editor SHALL accept `--rhi <vulkan|dx12|metal>` (default Vulkan) and
  pass the chosen API to the renderer.
- **Fix the DX12 gaps reached by the startup path** so the editor runs on DX12:
  - bind the pipeline root signature in `BindPipeline` (graphics + compute);
  - set the command-list primitive topology in `BindPipeline`;
  - give each command buffer a dedicated command allocator (was shared per pool, which reset an in-flight buffer's
    allocator and removed the device);
  - expose the backend clip-space Y axis on the device (`DeviceCapability::clipSpaceYDown`) and use it so the UI
    projection is upright on DX12/Metal as well as Vulkan;
  - surface DX12 debug-layer messages on pipeline-state creation failure and the device-removed reason on map
    failure.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
- `aurora-rhi-core`: the device exposes the active RHI API.
- `aurora-dx12-rendering`: the debug layer breaks only with a debugger attached.
- `ui-render`: the UI shader compilation target follows the active backend.
- `editor-render`: the editor renderer initializes with a selectable RHI API.

## Impact

- `engine/aurora/rhi/interface/**` (Device API accessor + `clipSpaceYDown` capability; `API` enum moved to
  `Core.h`).
- `engine/aurora/rhi/vulkan`, `engine/aurora/rhi/metal` (report the API from their device).
- `engine/aurora/rhi/dx12/**` (debug break policy, `D3D12Encoder` root signature + topology, `D3D12CommandPool`
  per-buffer allocator, `D3D12PipelineState` diagnostics + topology, `D3D12Buffer` map-failure reason,
  `D3D12Conversion` topology value).
- `engine/ui/render/**` (`UIRenderer` target selection, vertex semantics, compiler reflection).
- `engine/sandbox/module/src/SandboxModule.cpp`, `engine/sandbox/render/**/EditorRenderer.{h,cpp}` (`--rhi`).
