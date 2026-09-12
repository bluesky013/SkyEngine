## Context

现状（`dev_refactor_rhi`）：

- `PipelineLayout`（interface）是薄 `RefObject`，`Descriptor { groups: vector<ResourceGroupLayout*>（按 set，允许 nullptr 占位）, pushConstants }`。
- `Device::CreatePipelineLayout(desc)` 四后端声明；DX12/Metal 返回 nullptr stub。
- Vulkan：`VulkanPipelineLayout` 包装 `VkPipelineLayout`，但 `VulkanShader::CreatePipelineLayout()` 已独立在 shader 内创建 `VkPipelineLayout`（当前空 layout）；`VulkanPipelineState::BuildPipeline` 优先 `desc.layout`，否则 `vkShader->GetPipelineLayout()`。
- `GraphicsPipeline::Descriptor::layout` / `ComputePipeline::Descriptor::layout` 可选（nullptr → 空 layout）。
- shader 反射（`aurora/shader` 的 `ShaderReflection`，含 block 的 set/binding/type/stage）已是资源布局单一事实源（`aurora-shader-header-codegen` 落地）。

约束：`ResourceGroupLayout` / `ResourceGroup` 属 `aurora-resource-group` change（实质实现未落地，当前 CreateResourceGroup 返回 nullptr）；本 change 只动 `PipelineLayout`。

## Goals / Non-Goals

**Goals:**

- 删除 `PipelineLayout` 接口对象与 `Device::CreatePipelineLayout`，pipeline 描述符不再携带 layout。
- 确立「native pipeline layout 由各后端在 `Shader` 内从 shader 反射派生」的接口形态。
- push constants 归入 shader 反射，删除 `PipelineLayout::Descriptor::pushConstants`。
- Vulkan 行为与现状一致（空 layout），但入口统一到 `Shader`。

**Non-Goals:**

- 不实现 DX12 root signature / Metal argument buffer 的真实填充（`aurora-resource-group` 范畴）。
- 不实现 `ResourceGroup` / `ResourceGroupLayout` 的实质内容。
- 不实现 shader 反射到 `VkDescriptorSetLayout` 的完整填充（本 change 仅保持空 layout 现状，填充作为后续项）。

## Decisions

### 1. 移除 `PipelineLayout`，布局归 `Shader`

`PipelineLayout` 无实质功能（Vulkan 空壳 + DX12/Metal stub），且布局可由 shader 反射派生。删除后，native layout 由后端在 `Shader::Init` 时自建：

- Vulkan：`VulkanShader::CreatePipelineLayout()`（已有）继续产出 `VkPipelineLayout`，`GetPipelineLayout()` 供 pipeline state 使用。
- DX12：root signature 由 shader 反射派生（stub，后续填充）。
- Metal：argument buffer 由 shader 反射派生（stub，后续填充）。

**备选**（保留 PipelineLayout 作为显式 override）被否：与「shader 反射单一事实源」冲突，且当前显式 override 无人使用。

### 2. push constants 归入 shader 反射

`PipelineLayout::Descriptor::pushConstants` 删除。push-constant 区间（offset/size/stages）由 shader 声明并经反射得到，与 descriptor set/binding 同源。Vulkan 侧后续在 `CreatePipelineLayout()` 中由反射填充 `VkPushConstantRange`（当前空）。

### 3. `ResourceGroupLayout` 保留，来源改为反射

`ResourceGroupLayout` 仍是 `ResourceGroup` 的绑定描述，但不再被 `PipelineLayout::Descriptor::groups` 聚合。它的 `Descriptor` 由 shader 反射（`RgBlockDesc` → `ResourceGroupLayout`）产出。二者关系：`Shader`（反射）→ `ResourceGroupLayout`（接口对象，供 CreateResourceGroup）与 → 后端内部 native layout。

### 4. Pipeline 描述符去 layout 字段

`GraphicsPipeline::Descriptor` / `ComputePipeline::Descriptor` 删除 `layout` 字段。`VulkanPipelineState` 统一走 `vkShader->GetPipelineLayout()`，删除 `desc.layout` 分支。空 layout 场景由 shader 无反射资源自然产生。

### 5. 测试清理

`ResourceGroupTest.cpp` 删除 `PipelineLayoutEmpty` / `PipelineLayoutWithGroups` / `PipelineLayoutTooManyGroupsRejected`。`PipelineLayoutTooManyGroupsRejected` 里的 `MAX_RESOURCE_GROUPS` 上限校验逻辑随 `PipelineLayout.h` 一并移除。

## Risks / Trade-offs

- **[布局正确性依赖 shader 反射]**：`VulkanShader` 若从反射构建 descriptor set layout，需要反射完整（set/binding/stage）。当前 `VulkanShader::CreatePipelineLayout()` 仍为空，本 change 不引入新反射依赖，风险为零（仅删除 + 入口统一）。
- **[push constants 迁移]**：当前无使用者，删除字段不破坏行为；后续接反射时再补。
- **[DX12/Metal 失去 stub 占位]**：删除 `CreatePipelineLayout` stub 后，这两后端无布局入口，直到 `aurora-resource-group` 实现 root signature / argument buffer。当前这两后端 pipeline 创建本就因 stub 暂缓，无回归。

## Migration Plan

1. 删除 interface `PipelineLayout.h` + `Device::CreatePipelineLayout` + pipeline descriptor `layout` 字段。
2. 删除 Vulkan `VulkanPipelineLayout.{h,cpp}` + `VulkanDevice` 实现；`VulkanPipelineState` 去掉 `desc.layout` 分支。
3. 删除 DX12/Metal `CreatePipelineLayout` stub。
4. 清理 `ResourceGroupTest.cpp` PipelineLayout 用例；全量构建 + ctest。
5. 待用户确认后 `openspec archive aurora-shader-derived-layout`。

## Open Questions

- push constants 的反射字段放 `ShaderReflection`（aurora/shader）还是 `Shader`（rhi interface）？（倾向 `ShaderReflection`，随 `.slang` 反射产出，后端转换使用）
- `MAX_RESOURCE_GROUPS` 常量是否保留（仍可用于 ResourceGroup 数量上限），还是随 PipelineLayout 一并删除？（倾向保留，ResourceGroup 仍需上限）
