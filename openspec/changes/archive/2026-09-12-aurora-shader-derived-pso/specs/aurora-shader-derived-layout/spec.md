## MODIFIED Requirements

### Requirement: 后端在 Shader 内派生 native layout

native pipeline layout SHALL 由各后端在 `Shader` 内从 shader 反射派生，而非由调用方经独立对象传入：

- Vulkan：`VulkanShader` 自建 `VkPipelineLayout`（`CreatePipelineLayout()`，由 `Shader::Descriptor.reflection` 构建 descriptor set layout + push constants），pipeline state 统一用 `VulkanShader::GetPipelineLayout()`
- DX12：root signature 由 `D3D12Shader` 从反射构建（`D3D12RootSignature`），pipeline state 统一用 `d3dShader->GetRootSignature()`
- Metal：argument buffer 由 `MetalShader` 从反射构建

#### Scenario: Vulkan pipeline 使用 shader 布局

- **WHEN** 创建 `VulkanGraphicsPipeline`/`VulkanComputePipeline`（`Descriptor` 无 layout）
- **THEN** `VkGraphicsPipelineCreateInfo::layout` / `VkComputePipelineCreateInfo::layout` 取自 `vkShader->GetPipelineLayout()`，该 layout 由反射构建
