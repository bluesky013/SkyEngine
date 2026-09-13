# aurora-shader-derived-resource-group Specification

## Purpose
TBD - created by archiving change aurora-remove-resource-group-layout. Update Purpose after archive.
## Requirements
### Requirement: ResourceGroup 由 shader + set 创建

`ResourceGroup::Descriptor` SHALL 含 `Shader *shader` 与 `uint32_t set`，不再含 `ResourceGroupLayout *layout`。`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 从 shader 反射派生该 set 的 binding 布局并分配 ResourceGroup。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。

#### Scenario: 用 shader 创建 ResourceGroup

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`，其 binding 布局与 shader reflection 的 set 0 一致

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr，且 logger 报错

### Requirement: shader reflection 的 null 与 empty 语义

`Shader::Descriptor::reflection` 是 `const ShaderReflection*`，MUST 非 null：shader 创建与 `CreateResourceGroup` 遇 null reflection SHALL 拒绝并 logger 报错。非 null 但 `resources` / `pushConstants` / `blocks` 均为空的"空 reflection" SHALL 合法，对应无资源 shader 的空 set 布局。

#### Scenario: null reflection 拒绝

- **WHEN** `Shader::Descriptor::reflection = nullptr` 创建 shader 或 `CreateResourceGroup({shader, set})` 且 shader 无 reflection
- **THEN** 返回 nullptr 并 logger 报错

#### Scenario: 空 reflection 合法

- **WHEN** shader 的 reflection 非 null 但 `resources` 为空，调用 `CreateShader`
- **THEN** shader 创建成功（空 reflection 不报错）；随后 `CreateResourceGroup({shader, set})` 对该 shader 中不存在的 set 返回 nullptr

### Requirement: ResourceGroup 复用 shader 派生的 native set layout

各后端 ResourceGroup SHALL 复用 shader 内已派生的 native layout，而非自建：

- Vulkan：`VulkanShader` SHALL 暴露 `VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set)`，`VulkanResourceGroup` 用它 `vkAllocateDescriptorSets`。
- DX12：`D3D12ResourceGroup` SHALL 从 shader reflection 计算 set 的 descriptor 数量并分配 heap 区间。
- Metal：`MetalResourceGroup` SHALL 从 shader 派生的 argument buffer 布局分配（后续实现）。

#### Scenario: Vulkan descriptor set 与 pipeline layout 同源

- **WHEN** 用同一 shader 创建 pipeline 与 ResourceGroup
- **THEN** ResourceGroup 的 `VkDescriptorSet` 与 pipeline 的 `VkPipelineLayout` 使用同一 `VkDescriptorSetLayout`（非 compatible 的两套）

### Requirement: ResourceGroup 布局等于 shader reflection 的完整 set

ResourceGroup 的 binding 集合 SHALL 等于 shader reflection 中该 set 的全部资源；SHALL NOT 存在"手写少于 shader 的 layout"。未 `Update` 的 binding 保持"未写入"（Vulkan 对应 null descriptor）。

#### Scenario: 布局即 shader 完整 set

- **WHEN** shader reflection 的 set 0 含 binding 0（UB）+ binding 1（sampled image），创建 ResourceGroup{shader, 0}
- **THEN** ResourceGroup 覆盖这两个 binding；只 Update binding 0 时 binding 1 保持未写入

### Requirement: shader 生命周期不因 ResourceGroup 延长

shader SHALL 是派生 native layout 的短命对象，创建完 pipeline 后即可释放，不因 ResourceGroup 而延长生命周期，也 SHALL NOT 需要 delayed release。ResourceGroup 存续期间其 native set layout MUST 存活（最小落地：ResourceGroup 持 shader 引用；native layout 独立所有权为后续优化）。

#### Scenario: shader 不因 ResourceGroup 而无限延长

- **WHEN** 创建 pipeline 与 ResourceGroup 后释放调用方 shader 引用
- **THEN** shader 生命周期由 pipeline 与 ResourceGroup 的引用共同决定，且无 delayed release 语义；ResourceGroup 存续期间 set layout 不悬空

### Requirement: UBO 类型按 tier 约定（global/pass static，batch dynamic）

`ShaderResourceType` SHALL 含 `UNIFORM_BUFFER_DYNAMIC` 与 `STORAGE_BUFFER_DYNAMIC` 变体。set 0（global）与 set 1（pass）的 UBO SHALL 为 static `UNIFORM_BUFFER`；set 2（batch）的 UBO SHALL 为 dynamic `UNIFORM_BUFFER_DYNAMIC`（配合 per-draw offset）。

DYNAMIC 变体非 SPIR-V 反射自然产物，SHALL 由上层 codegen 按 `RgBlockDesc.kind`（`CBUFFER_DYNAMIC`）在构建 reflection 时标记。

#### Scenario: batch 的 UBO 为 dynamic

- **WHEN** codegen 生成 batch block（`kind = CBUFFER_DYNAMIC`）的 reflection，其 set 2 的 resource 类型标记为 `UNIFORM_BUFFER_DYNAMIC`
- **THEN** ResourceGroup{shader, 2} 派生出的 layout 为 dynamic UBO，`BindResourceGroup(2, rg, 1, &offset)` 生效

#### Scenario: global/pass 的 UBO 为 static

- **WHEN** global（set 0）/ pass（set 1）block 的 reflection resource 类型为 `UNIFORM_BUFFER`
- **THEN** ResourceGroup{shader, 0} 与 {shader, 1} 的 UBO 为 static，无 dynamic offset

