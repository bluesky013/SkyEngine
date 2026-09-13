# aurora-shader-derived-layout Specification

## Purpose
TBD - created by archiving change aurora-shader-derived-layout. Update Purpose after archive.
## Requirements
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

- Vulkan：`VulkanShader` 自建 `VkPipelineLayout`（`CreatePipelineLayout()`，由 `Shader::Descriptor.reflection` 构建 descriptor set layout + push constants），pipeline state 统一用 `VulkanShader::GetPipelineLayout()`
- DX12：root signature 由 `D3D12Shader` 从反射构建（`D3D12RootSignature`），pipeline state 统一用 `d3dShader->GetRootSignature()`
- Metal：argument buffer 由 `MetalShader` 从反射构建

#### Scenario: Vulkan pipeline 使用 shader 布局

- **WHEN** 创建 `VulkanGraphicsPipeline`/`VulkanComputePipeline`（`Descriptor` 无 layout）
- **THEN** `VkGraphicsPipelineCreateInfo::layout` / `VkComputePipelineCreateInfo::layout` 取自 `vkShader->GetPipelineLayout()`，该 layout 由反射构建

### Requirement: push constants 归入 shader 反射

`PipelineLayout::Descriptor::pushConstants` SHALL 被删除；push-constant 区间（offset/size/stages）SHALL 由 shader 声明并经反射得到，与 descriptor set/binding 同源。

#### Scenario: 无 PipelineLayout push constants

- **WHEN** 尝试在 layout 描述符里声明 push constants
- **THEN** 无此字段（编译失败）；push constants 改由 shader 反射承载

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

