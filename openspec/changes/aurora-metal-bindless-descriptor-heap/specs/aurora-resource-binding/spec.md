# Delta: aurora-resource-binding

## ADDED Requirements

### Requirement: tier2 bindless 的模型与后端能力

tier2 bindless 的抽象 `DescriptorHeap` 当前是**索引型堆**（`VK_EXT_descriptor_heap` 语义：tex/buf/smp 各自 backing buffer + per-type stride + per-type index）。各后端 SHALL 按实际能力置 `DeviceFeature::descriptorHeap`，能力不可用时 `CreateDescriptorHeap` SHALL 返回 `nullptr` 且调用方退化为 tier1。

Metal 后端 SHALL NOT 假设存在 argument buffer（`[[id]]`）形态的堆；slang 对 Metal 的 bindless 走**指针/地址型**模型（`DescriptorHandle<T>` 在 Metal 上具 `T` 的布局、以 `device T*` 表示，可 `ulong` 化），其 compiler emit 已完成（slang #10842），但引擎尚未采用该 shader/RHI 模型。因此在采用 `DescriptorHandle<T>` 之前，Metal SHALL 保持 `descriptorHeap == false`。

#### Scenario: 能力不可用退化

- **WHEN** 后端未实现 tier2（如当前 Vulkan / Metal）
- **THEN** `GetFeature().descriptorHeap == false` 且 `CreateDescriptorHeap` 返回 `nullptr`

#### Scenario: 能力可用

- **WHEN** 后端实现了 tier2（如 DX12 SM6.6）
- **THEN** `GetFeature().descriptorHeap == true` 且 `CreateDescriptorHeap` 返回实例

#### Scenario: Metal 采用指针型 bindless 后

- **WHEN** 引擎改用 slang `DescriptorHandle<T>` 且 Metal 运行时实现句柄表
- **THEN** Metal 后端 SHALL 置 `descriptorHeap == true` 并按指针/地址型语义访问资源
