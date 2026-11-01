## Context and goals

The user requested independent Vulkan configuration, defaulting dynamic rendering on as a preference, with the actual mode determined at device initialization. RendererVulkanMode and DeviceFrameContextInitInfo.vulkanCompiler currently duplicate this choice, while VulkanDevice rejects hardware without the feature.

## Decisions

`VulkanConfig` is plain signature data in the RHI interface, not a service singleton. RendererInitInfo owns one `vulkan` value and forwards it unchanged to the trailing `Instance::Descriptor::vulkan`. VulkanInstance retains this configuration for its device factory. Launcher/editor command-line handling sets the preference without selecting a compiler.

VulkanDevice records requested, supported and enabled/usable state. Requested=false or unsupported feature chooses traditional RenderPass. Missing dynamic command entry points also choose RenderPass after device dispatch loading; the enabled getter denotes the usable rendering path. Other required features still fail explicitly. Diagnostic logging reports the resolved state. Upstream Vulkan 1.1+ negotiation remains intact: individual feature structures, advertised KHR dependencies and version-correct command suffixes are preserved. A disabled preference keeps physical support distinct while omitting dynamic rendering from the enable chain; RenderPass fallback requires actual RenderPass2 support and commands.

VulkanDeviceFrameContext chooses its private compiler from the device's resolved state. Remove both public compiler-mode enums/fields so the same device cannot be given contradictory rendering modes by different contexts. Tests that exercise both paths create separate device sessions with different preferences.

The legacy graphics encoder and pipeline entry without explicit subpass are dynamic-only. They return unavailable when that path is disabled. The FrameGraph RenderPass compiler continues to use its explicit subpass pipeline path. Do not build a new legacy RenderPass adapter as part of this change.

## Ownership and compatibility

Keep existing targets and source owners. The new configuration header follows the existing RHI interface include path. `--aurora-vulkan-render-pass` remains a false preference override. D3D12/Metal ignore Vulkan configuration data and no longer validate a Vulkan context enum. There is no UI/config form in this change.

Process-resident DLL/module-lease cleanup is a subsequent change, as requested; it is not mixed into this Vulkan commit.

## Validation

Migrate existing compiler, upload, raster, runtime and editor native fixtures. Add native device-configuration tests loading the real Vulkan factory, intercepting supported-feature/device-dispatch queries to simulate unsupported features and missing commands, and completing raster-clear/copy/readback on the resulting device. Check disabled legacy encoder behavior. Reconfigure the existing VS build for the new header/test file and run source-ownership checks plus real affected libraries/modules/tests.
