## Why

Renderer and Launcher currently select Vulkan frame-graph compilers through two public enums. That backend policy belongs to device configuration. Moreover, Vulkan device creation rejects missing dynamicRendering support even though a traditional RenderPass implementation exists.

## What Changes

- Introduce independent `VulkanConfig` with `dynamicRendering=true` as the default preference.
- Forward it through the existing renderer/instance initialization without a mutable global configuration service.
- Resolve requested, supported and usable dynamic rendering during Vulkan device initialization; false, unsupported features or missing commands select RenderPass.
- Make Vulkan FrameContext consume the resolved device mode, removing renderer/context compiler-mode enums.
- Preserve the existing explicit RenderPass CLI as a configuration override and migrate editor/test callers.
- Reject legacy dynamic-only encoder/pipeline use when the feature is not enabled.

## Capabilities

### New Capabilities
- `vulkan-device-configuration`: Backend-specific preferences resolved against device capabilities.

### Modified Capabilities

None.

## Impact

Existing Aurora RHI, renderer and editor targets only; no new target or module entry. A new native test file belongs solely to the existing GPU test target. Interface signatures and existing test fixtures are migrated together. Synchronization2/timeline requirements and supported API versions are unchanged.
