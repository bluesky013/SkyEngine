## Why

Aurora 的 `ResourceGroup` 与 `ResourceGroupLayout` 当前**完全是空壳**：`ResourceGroup::Descriptor` 是空 struct，`ResourceGroupLayout::handlers` 是 private 成员且没有任何 setter，`Device::CreateSampler(ResourceGroup::Descriptor)`（注：这是个 typo，正确名字应为 `CreateResourceGroup`）在所有 4 个后端均返回 `nullptr`，`VulkanEncoder::BindResourceGroup` 是 `// TODO`。整条 descriptor binding 链路不可用——即使 Submit 与 Barrier 都接通了，aurora 仍然只能跑无任何资源绑定的 clear-screen。

Resource binding 是 RHI 的核心抽象之一，每个后端的描述符模型差异都很大（Vulkan descriptor set / DX12 root signature + descriptor heap / Metal argument buffer / GLES uniform 与 texture unit），需要一个统一的、能下沉到这四套模型的中间层。

## What Changes

- 完整化 **`ResourceGroupLayout`**：包含若干 `BindingDesc`（binding index、`DescriptorType`、`ShaderStageFlags`、count、binding flags），通过 `Device::CreateResourceGroupLayout(const Descriptor&)` 创建
- 完整化 **`ResourceGroup::Descriptor`**：含 `ResourceGroupLayout *layout`；group 创建后通过 `Update(const std::vector<ResourceUpdateInfo>&)` 写入实际资源（Buffer/Image/Sampler/CombinedImageSampler/...）
- 重命名 `Device::CreateSampler(ResourceGroup::Descriptor)` → **`Device::CreateResourceGroup`**（修复明显 typo）
- **`PipelineLayout`** 抽象：把多个 `ResourceGroupLayout`（按 set index 排）+ push constant ranges 聚合成一个 layout，作为 `GraphicsPipeline::Descriptor` / `ComputePipeline::Descriptor` 新的必填字段
- `Encoder::BindResourceGroup(uint32_t set, ResourceGroup *group)` 真正生效；与当前 pipeline 的 `PipelineLayout` 协同检查 set 索引合法性
- `Encoder::PushConstants(ShaderStageFlags, uint32_t offset, uint32_t size, const void*)` 配套加上
- 4 个后端落地：
  - Vulkan：`VkDescriptorSetLayout` + `VkDescriptorPool`（每 device 一个内部池，按需扩容）+ `VkDescriptorSet` + `VkPipelineLayout`
  - DX12：`ID3D12RootSignature` 由 `PipelineLayout` 构造；`ResourceGroup` 对应一段 descriptor table（CBV/SRV/UAV heap + sampler heap）
  - Metal：`MTLArgumentEncoder` + 显式 argument buffer；group 包装一个 `MTLBuffer`
  - GLES：layout 拍平为按 binding 索引数组；group 持有 binding → resource 映射，BindResourceGroup 时直接 `glBindBufferRange` / `glBindTextureUnit`
- 测试：单 group 单 binding（uniform buffer + sampled image）端到端绘制；动态偏移 uniform；多 set；count > 1 的 array binding；push constants

## Capabilities

### New Capabilities
- `aurora-resource-binding`: ResourceGroupLayout / ResourceGroup / PipelineLayout / push constants 与 Encoder::BindResourceGroup 联动
- `aurora-pipeline-layout`: PipelineLayout 抽象与现有 GraphicsPipeline / ComputePipeline 的集成

### Modified Capabilities
（无既有 spec 修改）

## Impact

- **接口头改动较大**：`aurora/rhi/ResourceGroup.h` 几乎重写；`aurora/rhi/PipelineState.h` 加 `PipelineLayout *layout` 必填；`aurora/rhi/Device.h` 加 `CreatePipelineLayout` / `CreateResourceGroupLayout` / 修复 `CreateResourceGroup`；`aurora/rhi/Encoder.h` 加 `PushConstants`
- **后端**：4 套 `*ResourceGroup` / `*ResourceGroupLayout` / `*PipelineLayout`；DX12 还要新增完整的 RootSignature 构造（当前是 stub）与 descriptor heap 管理；Metal 引入 argument buffer
- **测试**：新增 `ResourceGroupTest.cpp`；现有 `EncoderTest::CreateMinimalVulkanGraphicsPipeline` 需补 `PipelineLayout`
- **依赖**：本 change 依赖 `aurora-queue-submit-present`（要 Submit 才能跑端到端）与 `aurora-encoder-barriers`（compute UAV → graphics SRV 之类的 hazard 需要 barrier）
- **调用方**：暂无；后续 `engine/aurora/core/Renderer` 与 RDG 都将以 ResourceGroup 为基本绑定单元
