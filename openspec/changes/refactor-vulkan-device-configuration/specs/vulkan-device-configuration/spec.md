## ADDED Requirements

### Requirement: Vulkan preferences are independent device configuration

Vulkan dynamic rendering SHALL be requested through `VulkanConfig.dynamicRendering`, default true. Renderer/Launcher SHALL forward preferences without selecting a frame compiler. Frame contexts SHALL consume their device's resolved mode.

#### Scenario: Default supported device
- **WHEN** dynamic rendering is requested and the device supports the feature and required commands
- **THEN** the device selects dynamic rendering and its contexts use that path

#### Scenario: Explicit traditional rendering
- **WHEN** dynamicRendering is false
- **THEN** device initialization disables that rendering path and all its contexts select RenderPass

### Requirement: Device capability resolution supports fallback

Missing optional dynamic-rendering support or dispatch commands SHALL select traditional RenderPass rather than unconditionally rejecting device creation. Existing independent hard requirements SHALL remain enforced.

#### Scenario: Unsupported feature
- **WHEN** the physical device reports dynamicRendering=false while preference is true
- **THEN** device creation succeeds if other requirements are met
- **AND** a RenderPass frame can render and read back its result

#### Scenario: Missing dynamic command
- **WHEN** dynamic command dispatch is unavailable
- **THEN** contexts select RenderPass and never invoke the missing command

### Requirement: Disabled legacy dynamic APIs fail explicitly

Legacy graphics encoder creation and graphics pipeline initialization without an explicit render subpass SHALL fail when dynamic rendering is not enabled. Explicit subpass pipelines used by the RenderPass compiler SHALL remain functional.

#### Scenario: Disabled legacy encoder
- **WHEN** a caller requests the dynamic-only legacy graphics encoder on a RenderPass device
- **THEN** encoder creation returns unavailable without issuing unsupported native commands
