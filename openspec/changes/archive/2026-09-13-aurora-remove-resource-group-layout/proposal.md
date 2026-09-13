## Why

`PipelineLayout` 接口移除后，native binding layout（`VkPipelineLayout` / root signature / argument buffer）已统一由 shader reflection 派生；但 `ResourceGroup` 创建仍需要一个独立的 `ResourceGroupLayout` 对象，它与 shader reflection 承载**完全相同**的 binding 信息（set/binding/type/count），造成两套 `VkDescriptorSetLayout`（shader 派生的 private 一套 + 手写 layout 一套），一致性只能靠 codegen 的 `RgBlockDesc` 隐式保证，接口层无强制约束。

## What Changes

- **移除 `ResourceGroupLayout` 接口对象**（`aurora/rhi/ResourceGroup.h` 中删除 `ResourceGroupLayout` 类、`Device::CreateResourceGroupLayout`）。**BREAKING**。
- `ResourceGroup::Descriptor` 改为引用 shader + set index，ResourceGroup 直接从 shader reflection 派生的 binding 布局分配 descriptor。
- **清理 `DescriptorType` / `DescriptorBindingFlags` 枚举**：移除 `ResourceGroupLayout` 后这两个枚举失去唯一使用方；binding 类型统一用 `ShaderResourceType`。**DYNAMIC 保留**：`ShaderResourceType` 增加 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC`（global/pass 用 static UBO，batch 用 dynamic UBO）。**BREAKING**。
- **shader reflection 语义界定**：`Shader::Descriptor::reflection` MUST 非 null（null 即报错）；非 null 但 `resources` 为空的"空 reflection"合法（无资源 shader，如纯 push constant）。
- **shader 生命周期**：shader 是派生 layout 的输入，创建完 pipeline 后即可释放，不因 ResourceGroup 而延长（ResourceGroup 复用 shader 派生的 native set layout，持有 native 句柄而非 shader 对象）。
- 各后端复用 shader 内已派生的 native layout：
  - Vulkan：`VulkanShader` 暴露 per-set 的 `VkDescriptorSetLayout`，`VulkanResourceGroup` 用它 `vkAllocateDescriptorSets`。
  - DX12：`D3D12ResourceGroup` 从 root signature / reflection 计算 set 的 descriptor 数量，删除 `D3D12ResourceGroupLayout`。
  - Metal：`MetalShader` 从 reflection 派生 argument buffer 布局，ResourceGroup 复用（Metal 后端 ResourceGroup 尚未实现，本 change 只定接口）。
- 上层 `aurora/pipeline` 删除 `ToLayoutDescriptor(RgBlockDesc)` → `CreateResourceGroupLayout` 链路；global / pass 的 set 布局用 **dummy shader**（仅含对应 block 声明的最小 shader，编译得 reflection + native set layout）派生。
- `RgBlockDesc` 保留，仍作为 codegen 单一事实源（生成 C++ 镜像 struct + shader 头），但不再喂给 RHI。

## Capabilities

### New Capabilities

- `aurora-shader-derived-resource-group`: ResourceGroup 的 descriptor 布局从 shader reflection 派生（`{shader, set}`），不再依赖独立 ResourceGroupLayout 对象。

### Modified Capabilities

- `aurora-resource-binding`: 移除 `ResourceGroupLayout` 相关 requirement（`ResourceGroupLayout 描述一组绑定槽`、`CreateResourceGroup 按 Descriptor::layout` 等），ResourceGroup 创建改为引用 shader + set。
- `aurora-shader-derived-layout`: 扩展 requirement——shader 派生的 per-set descriptor set layout 不仅用于 pipeline layout，还作为 ResourceGroup 分配 descriptor 的唯一来源，并需暴露查询接口。

## Impact

- 接口层：`aurora/rhi/ResourceGroup.h`（删 `ResourceGroupLayout`、`DescriptorType` 引用）、`aurora/rhi/Core.h`（删 `DescriptorType` / `DescriptorBindingFlags`）、`aurora/rhi/Device.h`（删 `CreateResourceGroupLayout`）、`aurora/rhi/Shader.h`（明确 reflection null 语义）。
- Vulkan：`VulkanShader`（暴露 `GetDescriptorSetLayout(set)` + set 索引映射修复）、`VulkanResourceGroup`（复用）、删 `VulkanResourceGroupLayout`、`VulkanConversion.cpp` 删 `FromDescriptorType`。
- DX12：`D3D12Shader`（暴露 reflection）、`D3D12ResourceGroup`（从 reflection 计算）、`D3D12RootSignature`（`RootSignatureDescriptorRange.type` 改 `ShaderResourceType`）、删 `D3D12ResourceGroupLayout`、`D3D12Conversion.cpp` 删 `FromDescriptorType`。
- Metal：`MetalDevice` 删 `CreateResourceGroupLayout` stub。
- 上层：`aurora/pipeline`（`GlobalRenderResources` / `PipelinePass` 改用 dummy shader + `{shader, set}`）、`aurora/shader/RgBlockDesc.cpp`（删 `ToLayoutDescriptor`）。
- 测试：`ResourceGroupTest` 改为用 shader 创建 ResourceGroup；`RgBlockDescTest` / `ResourceTiersTest` 删 layout 转换用例。
