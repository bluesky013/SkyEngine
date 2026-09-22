## ADDED Requirements

### Requirement: Device 暴露当前 RHI API

Aurora `Device` SHALL expose the active RHI `API` (through `Device::GetAPI()`), implemented by every backend, so
consumers can adapt behavior (for example the shader compilation target) to the backend actually in use.

#### Scenario: Query the active API
- **WHEN** a consumer holds a `Device*` created through `Instance::Init`
- **THEN** `Device::GetAPI()` SHALL return the backend in use (`VULKAN` / `DX12` / `METAL`)

#### Scenario: Backends implement the accessor
- **WHEN** any of the Vulkan, DX12 or Metal backends is built
- **THEN** its device SHALL implement `GetAPI()` and SHALL NOT fall back to a default value

### Requirement: Device 暴露 clip-space Y 方向

Aurora `Device` SHALL expose the clip-space Y axis of the active backend (`DeviceCapability::clipSpaceYDown`),
filled by each backend in `UpdateDeviceCaps()`, so projection builders can produce the correct vertical orientation
on every backend.

#### Scenario: Vulkan is Y-down
- **WHEN** the active backend is Vulkan
- **THEN** `clipSpaceYDown` SHALL be `true`

#### Scenario: DX12 and Metal are Y-up
- **WHEN** the active backend is DX12 or Metal
- **THEN** `clipSpaceYDown` SHALL be `false`
