## MODIFIED Requirements

### Requirement: Backend-agnostic with Vulkan first

The UI render pass SHALL be written against Aurora interfaces only and SHALL compile its shaders to the target of
the active backend (SPIR-V for Vulkan, DXIL for DX12, MSL for Metal), so it runs on any backend whose Aurora
capabilities have landed.

#### Scenario: Runs on Vulkan
- **WHEN** the engine runs with the Vulkan backend and a UI tree is painted
- **THEN** the UI pass SHALL execute with SPIR-V bytecode and produce visible output

#### Scenario: Runs on DX12
- **WHEN** the engine runs with the DX12 backend and a UI tree is painted
- **THEN** the UI shader SHALL be compiled to DXIL and the UI pipeline state SHALL be created successfully

#### Scenario: Upright on every backend
- **WHEN** the UI is painted on a backend whose clip-space Y axis differs from another's
- **THEN** the UI SHALL be upright (pixel y=0 at the framebuffer top) on both, with the projection's Y sign taken
  from the device's clip-space Y axis

#### Scenario: Unsupported backend degrades gracefully
- **WHEN** the active backend lacks a required Aurora capability
- **THEN** the UI pass SHALL report the limitation without crashing
