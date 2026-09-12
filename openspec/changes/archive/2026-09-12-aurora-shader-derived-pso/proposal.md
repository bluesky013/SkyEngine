## Why

`aurora-shader-derived-layout` 已经删掉 `PipelineLayout` 接口、确立「布局在 `Shader` 内派生」的形态，但后端的布局派生仍是空壳：`VulkanShader::CreatePipelineLayout()` 只创建空 `VkPipelineLayout`（无 descriptor set layout、无 push constants）；`D3D12Shader::Init` 根本没建 `D3D12RootSignature`（`D3D12GraphicsPipeline`/`D3D12ComputePipeline` 却解引用 `GetRootSignature()`，PSO 创建因此是 stub）；`D3D12ComputePipeline::Init` 还残留一个外部 `rootSig` 参数。根本原因：shader 反射（`aurora/shader` 的 `ShaderReflection`，含 set/binding/type/stage）没有流进 RHI `Shader`，后端拿不到布局信息。本 change 打通反射链路，让 Vulkan `VkPipelineLayout` 与 DX12 root signature 直接由 shader 反射创建维护。

## What Changes

- **反射进入 RHI `Shader`**：把后端无关的 `ShaderReflection`（set/binding/type/stage）下沉到 `aurora/rhi` 接口层（`aurora/rhi/ShaderReflection.h`），`ShaderFunction::Descriptor`（或 `Shader`）携带反射；`aurora/shader` 编译器填它，后端消费它。
- **Vulkan**：`VulkanShader::CreatePipelineLayout()` 由反射构建 `VkDescriptorSetLayout`（按 set 分组）+ `VkPushConstantRange` + `VkPipelineLayout`，替换空 layout。
- **DX12**：`D3D12Shader::Init` 由反射构建 `D3D12RootSignature`（复用现有 `RootSignatureDescriptor` 的 range/set 生成逻辑，改由反射填）；`D3D12GraphicsPipeline`/`D3D12ComputePipeline` 统一走 `d3dShader->GetRootSignature()`，删除 `D3D12ComputePipeline::Init` 的外部 `rootSig` 参数。
- **Metal**：`MetalShader` 由反射构建 argument buffer（macOS only，尽力实现或 stub 保持一致）。
- **PSO 流程闭环**：三后端 `CreatePipelineState` 使用 shader 内部维护的 native layout，不再有外部 layout 依赖。

## Capabilities

### New Capabilities

- `aurora-shader-derived-pso`: 各后端 native pipeline layout / root signature 直接由 shader 反射创建维护（Vulkan `VkPipelineLayout`、DX12 root signature、Metal argument buffer），PSO 创建只依赖 shader。

### Modified Capabilities

- `aurora-shader-derived-layout`: 其「后端在 Shader 内派生 native layout」需求从「空 layout」落实为「由 shader 反射填充」的实际实现。

## Impact

- **engine/aurora/rhi/interface**：新增 `ShaderReflection.h`（从 `aurora/shader` 迁移，或新定义 RHI 级反射结构）；`Shader.h`/`ShaderFunction` 携带反射。
- **engine/aurora/shader**：`ShaderCompilerSlang`/`ShaderCodeGen` 改为引用 RHI 层反射；`ShaderReflection.h` 迁出。
- **engine/aurora/rhi/vulkan**：`VulkanShader` 由反射建 descriptor set layout + pipeline layout；`VulkanShaderFunction` 携带反射。
- **engine/aurora/rhi/dx12**：`D3D12Shader` 由反射建 root signature；`D3D12PipelineState` 去掉外部 rootSig 参数。
- **engine/aurora/rhi/metal**：`MetalShader` 由反射建 argument buffer。
- **范围边界**：不实现 descriptor 的实际绑定/更新（`aurora-resource-group` 范畴）；本 change 只让 layout/root signature/argument buffer 从 shader 反射派生。
