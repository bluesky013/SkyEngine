# Verification — 2026-10-30

## Implementation

- Independent `VulkanConfig.dynamicRendering` defaults true and is forwarded through RendererInitInfo and Instance::Descriptor.
- VulkanDevice reports requested, supported and usable/enabled state. Missing feature support or dynamic commands selects RenderPass without rejecting an otherwise supported device.
- FrameContext reads the device's resolved mode; neither public mode enum nor the duplicate context option remains.
- Launcher/editor retain the existing explicit RenderPass CLI as a false preference override.
- Dynamic-only legacy encoder/pipeline creation fails explicitly on a RenderPass device.
- Existing rendering fixture modes are selected before creating the device; context isolation tests retain ownership checks.

## Build and source ownership

Reconfigured the existing VS 2022 Debug build after adding the configuration header and native test source, preserving `3RD_PATH=D:/Code/Engine/sky_3rd/Win32`. Built `Aurora.RHI.Static`, `Aurora.Vulkan`, `Aurora.Core.Static`, `Aurora.Render`, `EditorRender`, `SandboxModule`, and `Aurora.Renderer.GpuSubmissionTest`, including their actual adaptor/pipeline/backend dependencies.

`python python/tests/test_cmake_source_ownership.py`: 26 passed. The new native test source has one test owner; production sources retain their existing owners. No new target or compile variant was added.

## Native regressions

The broad selection ran 584 cases: **574 passed, 10 skipped, 0 failures**. An additional resource-state selection passed **48/48**, for **622 passed, 10 skipped, 0 failures** total without counting the repeated standalone configuration run.

| Suite | Cases | Skipped |
| --- | ---: | ---: |
| EditorHostNativeTest | 9 | 0 |
| RendererRuntimeTest | 162 | 0 |
| FrameOutputs | 123 | 10 |
| CopyGraph | 24 | 0 |
| VulkanFrameCompilerFixture | 58 | 0 |
| VulkanDeviceConfigTest | 4 | 0 |
| FrameAllocations | 15 | 0 |
| ResourceUploads | 72 | 0 |
| IndexedRaster | 48 | 0 |
| ImageSubresource | 15 | 0 |
| FrameTransient | 54 | 0 |
| ResourceStates | 48 | 0 |

All skips are Vulkan-specific output tests instantiated with the D3D12 parameter.

The new native device test loads the actual Vulkan factory and covers default-on, explicit-off, masked dynamic feature support and missing dynamic command entry points. Each case executes a raster clear, image-to-buffer copy and pixel readback. Disabled cases also verify legacy graphics encoder creation returns unavailable. Function-table probes and device/instance/library teardown preserve ownership.

## Static checks and scope

- Strict OpenSpec validation and `git diff --check` passed.
- Changed C++ files were formatted with the repository clang-format policy.
- Repository search found no remaining old public mode enum or context option in production/test callers.
- The prior changes were committed as `6f5d19e3`; this Vulkan change is a separate commit as requested.
- Process-resident loader/module-lease cleanup follows this change. No code-lease cleanup is mixed into this diff.

Evidence: `build/vulkan-device-config-review/{configure.log,build.log,source-ownership.log,device-config-tests.xml,native-regressions.xml,resource-states.xml}`. GPU tests ran on the local RTX 3080 with Vulkan dynamic/RenderPass and D3D12 coverage; no Metal device run is claimed.
