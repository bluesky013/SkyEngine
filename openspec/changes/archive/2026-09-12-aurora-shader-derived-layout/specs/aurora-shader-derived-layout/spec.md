## ADDED Requirements

### Requirement: PipelineLayout 接口对象被移除

RHI 接口层 SHALL 不暴露 `PipelineLayout` 对象：不存在 `aurora/rhi/PipelineLayout.h`，`Device` SHALL 无 `CreatePipelineLayout`，`GraphicsPipeline::Descriptor` 与 `ComputePipeline::Descriptor` SHALL 无 `layout` 字段。

#### Scenario: 无 PipelineLayout 符号

- **WHEN** 调用方 include `<aurora/rhi/PipelineLayout.h>` 或 `device->CreatePipelineLayout({})`
- **THEN** 编译失败（符号已删除）

#### Scenario: pipeline 描述符无 layout

- **WHEN** 构造 `GraphicsPipeline::Descriptor{}` / `ComputePipeline::Descriptor{}`
- **THEN** 描述符不含 layout 字段；pipeline 创建只依赖 shader

### Requirement: 后端在 Shader 内派生 native layout

native pipeline layout SHALL 由各后端在 `Shader` 内从 shader 反射派生，而非由调用方经独立对象传入：

- Vulkan：`VulkanShader` 自建 `VkPipelineLayout`（`CreatePipelineLayout()`），pipeline state 统一用 `VulkanShader::GetPipelineLayout()`
- DX12：root signature 由 shader 反射派生
- Metal：argument buffer 由 shader 反射派生

#### Scenario: Vulkan pipeline 使用 shader 布局

- **WHEN** 创建 `VulkanGraphicsPipeline`/`VulkanComputePipeline`（`Descriptor` 无 layout）
- **THEN** `VkGraphicsPipelineCreateInfo::layout` / `VkComputePipelineCreateInfo::layout` 取自 `vkShader->GetPipelineLayout()`，无 `desc.layout` 分支

### Requirement: push constants 归入 shader 反射

`PipelineLayout::Descriptor::pushConstants` SHALL 被删除；push-constant 区间（offset/size/stages）SHALL 由 shader 声明并经反射得到，与 descriptor set/binding 同源。

#### Scenario: 无 PipelineLayout push constants

- **WHEN** 尝试在 layout 描述符里声明 push constants
- **THEN** 无此字段（编译失败）；push constants 改由 shader 反射承载
