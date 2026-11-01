## ADDED Requirements

### Requirement: OHOS surface in the existing Vulkan backend
Aurora SHALL create an OHOS Vulkan surface inside the existing `Aurora.Vulkan` backend using `VK_OHOS_surface`, `vkCreateSurfaceOHOS` and the window's live `OHNativeWindow`. Headers and native declarations SHALL be compatible with the selected SDK. Required extensions, function pointers and queue presentation support SHALL be checked explicitly.

#### Scenario: Supported native window
- **WHEN** an OHOS window and all required instance/device capabilities are available
- **THEN** the existing Vulkan backend SHALL create its surface and swapchain from that window and present through the normal RendererHost path

#### Scenario: Unsupported surface extension or entry point
- **WHEN** the loader omits the OHOS surface extension or its creation function
- **THEN** initialization SHALL fail with a diagnostic identifying the missing capability and release partially created objects

### Requirement: Preserve frame capability and retirement contracts
OHOS SHALL meet the selected Vulkan compiler and checked SwapChain capability requirements, including applicable synchronization and presentation-retirement support. Unsupported capabilities SHALL be reported explicitly; the platform SHALL NOT silently switch to the legacy frame protocol or claim support from SDK header availability.

#### Scenario: Optional swapchain maintenance extension absent
- **WHEN** the device supports core swapchain and renderer synchronization but omits maintenance1
- **THEN** the existing Vulkan backend SHALL select its checked core WSI compatibility path without enabling unavailable extensions, preserving per-image semaphore ownership, deferred generation retirement and explicit shutdown limitations; OHOS-specific Surface code SHALL NOT implement a second renderer

#### Scenario: Required device feature absent
- **WHEN** the physical device lacks a feature required by the selected frame/presentation path
- **THEN** startup SHALL report the missing feature and SHALL NOT enter a frame loop that cannot safely retire submissions

#### Scenario: Resize or surface loss with work in flight
- **WHEN** resize, backgrounding or surface destruction occurs with GPU or presentation work in flight
- **THEN** the host SHALL stop acquiring the old output, preserve submitted resource lifetimes until safe retirement, and resume only on the current generation

#### Scenario: No drawable acquired
- **WHEN** a frame cannot acquire a usable output
- **THEN** the frame SHALL follow the existing skip/discard protocol without waiting on an unsignaled fence or presenting an invalid image

### Requirement: Real device rendering evidence
HarmonyOS Vulkan acceptance SHALL include actual device capability output, rendered content and lifecycle results. Cross-compilation, ELF inspection and generic desktop tests SHALL be recorded separately from device acceptance.

#### Scenario: SDK-only validation
- **WHEN** only SDK compilation and native library linking have been executed
- **THEN** the record SHALL mark Vulkan runtime, presentation and device lifecycle acceptance as unverified
