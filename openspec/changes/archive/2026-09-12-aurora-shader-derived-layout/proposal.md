## Why

`PipelineLayout` 目前是 RHI 接口里的冗余占位对象：`Descriptor` 只装 `ResourceGroupLayout*` 数组 + push constants，而 Vulkan 侧 `VulkanPipelineLayout` 只是空壳包装，`VulkanShader::CreatePipelineLayout()` 其实已经在 shader 内部自建 `VkPipelineLayout`（`VulkanPipelineState` 也已经「优先 `desc.layout`、否则回退 `vkShader->GetPipelineLayout()`」）。DX12 / Metal 的 `CreatePipelineLayout` 都是返回 nullptr 的 stub。既然 shader 反射（`.slang` → `ShaderReflection`，含 set/binding/type/stage）已经是资源布局的单一事实源，`PipelineLayout` 作为独立对象既无实质功能、又强迫调用方手工拼装 layout，是纯复杂度。

## What Changes

- **移除 `PipelineLayout` 接口对象**：删除 `aurora/rhi/PipelineLayout.h`、`Device::CreatePipelineLayout`、`VulkanPipelineLayout`，以及 `GraphicsPipeline::Descriptor::layout` / `ComputePipeline::Descriptor::layout` 字段。
- **布局由各后端在 `Shader` 内自维护**：native pipeline layout（VK 的 `VkPipelineLayout`、DX12 的 root signature、Metal 的 argument buffer）由后端在创建 shader 时从 shader 反射派生，不再经 `PipelineLayout` 手工传入。
- **push constants 移入 shader 反射**：`PipelineLayout::Descriptor::pushConstants` 删除，push-constant 区间由 shader 声明并经反射得到（与 descriptor set/binding 同源）。
- **Vulkan 落实现状对齐**：`VulkanPipelineState` 去掉 `desc.layout` 分支，统一走 `vkShader->GetPipelineLayout()`；`VulkanShader::CreatePipelineLayout()` 后续按反射填充 descriptor set layout + push constants（当前为空 layout，行为不变）。
- **清理测试**：删除 `ResourceGroupTest.cpp` 里 PipelineLayout 的 empty/groups/too-many-groups 用例。
- **`ResourceGroupLayout` 保留**：仍是创建 `ResourceGroup` 所需的绑定描述，但它的来源变成 shader 反射（`RgBlockDesc` → `ResourceGroupLayout`），不再被 `PipelineLayout` 聚合。

## Capabilities

### New Capabilities

- `aurora-shader-derived-layout`: RHI 不再暴露 `PipelineLayout` 对象；native pipeline layout 由各后端在 `Shader` 内从 shader 反射派生（含 push constants），pipeline 描述符不再携带 layout。

### Modified Capabilities

- `aurora-rhi-conventions`: `Device` 创建入口集合不再包含 `CreatePipelineLayout`（对应 `CreateResourceGroup` 正确入口的约定保持一致）。

## Impact

- **engine/aurora/rhi/interface**：删除 `PipelineLayout.h`；`Device.h` 去掉 `CreatePipelineLayout`；`PipelineState.h` 去掉 `layout` 字段。
- **engine/aurora/rhi/vulkan**：删除 `VulkanPipelineLayout.{h,cpp}`；`VulkanDevice` 去掉实现；`VulkanPipelineState` 统一走 shader 布局；`VulkanShader` 布局派生逻辑承载 push constants + descriptor set layout。
- **engine/aurora/rhi/dx12、metal**：删除 `CreatePipelineLayout` stub 声明/实现。
- **engine/aurora/rhi/test**：`ResourceGroupTest.cpp` 移除 PipelineLayout 用例。
- **范围边界**：不实现 DX12 root signature / Metal argument buffer 的实际填充（留待 `aurora-resource-group`），本 change 只确立「布局在 Shader 内、由反射派生」的接口形态。
