## ADDED Requirements

### Requirement: shader 派生的 set layout 供 ResourceGroup 复用

shader 内派生的 per-set descriptor set layout SHALL 不仅用于 pipeline layout，也作为 ResourceGroup 分配 descriptor 的唯一来源。

- Vulkan：`VulkanShader` SHALL 暴露 per-set 的 `VkDescriptorSetLayout`（按真实 set 索引查询），`VulkanResourceGroup` 用它 `vkAllocateDescriptorSets`，保证 `VkDescriptorSet` 与 `VkPipelineLayout` 同源。
- DX12：`D3D12Shader` SHALL 暴露 reflection，`D3D12ResourceGroup` 从 reflection 计算 set 的 descriptor 数量。
- Metal：`MetalShader` SHALL 从反射派生 argument buffer 布局供 ResourceGroup 复用（后续实现）。

#### Scenario: Vulkan set layout 按真实 set 索引查询

- **WHEN** shader reflection 含 set 0 与 set 2（set 索引有空洞）
- **THEN** `GetDescriptorSetLayout(0)` 与 `GetDescriptorSetLayout(2)` 返回各自正确的 per-set layout，而非按数组下标错位

#### Scenario: ResourceGroup 复用 shader set layout

- **WHEN** 用同一 shader 分别创建 pipeline 与 ResourceGroup{shader, set}
- **THEN** ResourceGroup 的 descriptor set 与 pipeline layout 的对应 set 使用同一 `VkDescriptorSetLayout`
