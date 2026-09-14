## ADDED Requirements

### Requirement: SwapChain 状态自查

`SwapChain` SHALL 提供 `SwapChainStatus GetStatus() const`，枚举 `{ OK, OUT_OF_DATE, LOST }`。`GetStatus()` SHALL 由后端自查：surface 尺寸由原生窗口决定，后端查询 `Descriptor.window` 当前尺寸与自身 extent 比对。

- 一致 → `OK`；
- 不一致 → `OUT_OF_DATE`；
- surface 丢失 → `LOST`。

`SUBOPTIMAL` SHALL 当作 `OK` 处理。RHI 层 SHALL NOT 决定目标尺寸（尺寸来自原生窗口）。

#### Scenario: 尺寸一致返回 OK

- **WHEN** swapchain extent 与原生窗口当前尺寸一致
- **THEN** `GetStatus()` 返回 `OK`

#### Scenario: 尺寸不一致返回 OUT_OF_DATE

- **WHEN** 原生窗口被 resize，其尺寸与 swapchain extent 不一致
- **THEN** `GetStatus()` 返回 `OUT_OF_DATE`（调用方据此触发 `Resize`）

#### Scenario: surface 丢失返回 LOST

- **WHEN** surface 丢失（如 Vulkan `VK_ERROR_SURFACE_LOST_KHR`）
- **THEN** `GetStatus()` 返回 `LOST`
