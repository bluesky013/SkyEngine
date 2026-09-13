## 1. 接口层

- [x] 1.1 修改 `aurora/rhi/ResourceGroup.h`：完整重写
  - [x] 1.1.1 `ResourceGroupLayout::BindingDesc` + `Descriptor`
  - [x] 1.1.2 `ResourceGroup::Descriptor` 含 layout 指针
  - [x] 1.1.3 `ResourceUpdateInfo` + `ResourceWriteKind` 枚举（用 union-friendly POD 字段，非 union 因为成员含构造函数）
  - [x] 1.1.4 `ResourceGroup::Update(writes)` 虚接口
- [x] 1.2 新增 `aurora/rhi/PipelineLayout.h`：`PipelineLayout` 类 + `Descriptor` + `MAX_RESOURCE_GROUPS = 4` 常量
- [x] 1.3 修改 `aurora/rhi/PipelineState.h`：`GraphicsPipeline::Descriptor` / `ComputePipeline::Descriptor` 加 `PipelineLayout *layout`（**transitional**：当前是可选 nullptr 字段，让存量测试不破；spec 长期目标是必填，等所有 caller 迁移后收紧）
- [x] 1.4 修改 `aurora/rhi/Device.h`：
  - [x] 1.4.1 重命名（quick-fixes 已先行 land）
  - [x] 1.4.2 加 `CreateResourceGroupLayout` 与 `CreatePipelineLayout`
- [x] 1.5 修改 `aurora/rhi/Encoder.h`：
  - [x] 1.5.1 `BindResourceGroup` 加 `numDynamicOffsets` / `dynamicOffsets` 参数（默认值，旧 caller 兼容）
  - [x] 1.5.2 加 `PushConstants` 接口（Graphics 含 stages，Compute 不含）

## 2. Vulkan 后端

- [x] 2.1 新增 `VulkanResourceGroupLayout.h/.cpp`：`vkCreateDescriptorSetLayout`
- [x] 2.2 新增 `VulkanPipelineLayout.h/.cpp`：聚合多 set + push constants → `vkCreatePipelineLayout`；MAX_RESOURCE_GROUPS 校验
- [ ] 2.3 ~~新增 VulkanDescriptorPool~~：本轮采用每 group 一个独立 pool 的简化策略（满了开新 pool 的全局 pool 留 v2 优化）
- [x] 2.4 新增 `VulkanResourceGroup.h/.cpp`：`vkAllocateDescriptorSets` + `Update` 用 `vkUpdateDescriptorSets`；自带专属 pool
- [x] 2.5 修改 `VulkanGraphicsPipeline` / `VulkanComputePipeline`：`Descriptor::layout` 优先；fallback 到 shader-derived；新增 `GetLayoutHandle` 给 encoder 用
- [x] 2.6 实现 `VulkanGraphicsEncoder::BindResourceGroup` / `VulkanComputeEncoder::BindResourceGroup`：`vkCmdBindDescriptorSets` + dynamic offsets；BindPipeline 时记 `currentLayout`
- [x] 2.7 实现 `VulkanGraphicsEncoder::PushConstants` / `VulkanComputeEncoder::PushConstants`：`vkCmdPushConstants`
- [x] 2.8 函数指针表加 `vkCreateDescriptorSetLayout` 等 + `vkCmdPushConstants`

## 3. DX12 后端

- [x] 3.1 新增 `D3D12DescriptorAllocator`：全局 shader-visible CBV/SRV/UAV + sampler heap，free-list 分配
- [x] 3.2 新增 `D3D12ResourceGroupLayout`：binding 布局 + CBV/SRV/UAV / sampler descriptor 计数 + 去重/VARIABLE_COUNT 校验
- [x] 3.3 新增 `D3D12ResourceGroup`：heap 区间分配 + `Update` 写 descriptor（CBV/SRV/UAV/Sampler/Combined 拆 SRV+sampler）
- [x] 3.4 `D3D12Image::CreateSRV/CreateUAV` + `D3D12Buffer::CreateCBV/CreateSRV/CreateUAV`
- [x] 3.5 `D3D12RootSignature` 重构：sampler 拆独立 descriptor table + set→root param 映射 + push constant root param
- [x] 3.6 `D3D12Device::CreateResourceGroupLayout/CreateResourceGroup` 实现 + descriptor allocator 初始化
- [x] 3.7 `D3D12Encoder::BindResourceGroup`（SetDescriptorHeaps + SetGraphicsRootDescriptorTable）+ `PushConstants`（SetGraphicsRoot32BitConstants）
- [ ] 3.8 dynamic offset（`UNIFORM_BUFFER_DYNAMIC` 用 root CBV / root descriptor）——留下次
- [ ] 3.9 DX12 端到端绘制验证（BindResourceGroup + Draw + readback）——留下次（需 shader pipeline 完整调用链）

## 4. Metal 后端

- [ ] 4.1–4.7 全部留下次实施（接口 stub 已加，编译通过；argument buffer 在 macOS 实现）

## 5. GLES 后端

- [x] 5.1 已废弃：GLES 后端整体删除（commit 17bcc733），无需实现

## 6. 测试

- [ ] 6.1 ~~扩展 AuroraTestHelper~~（本轮未做；现有 helper 够用）
- [x] 6.2 新增 `ResourceGroupTest.cpp`（Vulkan）：7 条用例
  - [x] 6.2.1 `CreateEmptyLayoutAndGroup`
  - [x] 6.2.2 `LayoutWithUniformAndSampledImage`
  - [x] 6.2.3 `GroupRequiresLayout`（layout=null 拒绝）
  - [x] 6.2.4 `PipelineLayoutEmpty` / `PipelineLayoutWithGroups`
  - [x] 6.2.5 `PipelineLayoutTooManyGroupsRejected`（5 set）
  - [x] 6.2.6 `UpdateUniformBuffer`（不 crash）
  - [ ] BindAndDraw 端到端 / DynamicOffset / ArrayBinding / PushConstantsRoundTrip / MismatchedUpdate（需要 shader pipeline 完整调用链 + readback；留下次）
- [ ] 6.3 ~~修改 EncoderTest 工具补 PipelineLayout~~：layout 是可选 nullptr，测试无破坏
- [x] 6.4 后端 fixture：`ResourceGroupTest` 覆盖 Vulkan 4 用例 + DX12 5 用例（含 COMBINED_IMAGE_SAMPLER 拆分验证），9/9 通过

## 7. 收尾 / 文档

- [ ] 7.1 AGENTS.md "Resource binding 模型" 章节：留下次（待 DX12/Metal 实现完成）
- [x] 7.2 `AuroraTest --gtest_filter=ResourceGroup*` 7/7 PASSED；全集 102/102 PASSED 无回归
- [x] 7.3 Vulkan validation 不报新 warning
- [x] 7.4 archive：已归档（Vulkan + DX12 完成；Metal 后端 / dynamic offset / 端到端绘制验证留待后续 change）

## 8. DescriptorHeap 接口（tier2 heap，实现拆后续 change）

- [x] 8.1 新增 `DescriptorHeap.h`：`Descriptor`（per-type 容量）+ `Allocation`（per-type 索引）+ `Allocate`/`Free`/`Update`
- [x] 8.2 `DeviceFeature` 加 `descriptorHeap`
- [x] 8.3 `Device::CreateDescriptorHeap` 接口
- [x] 8.4 `Encoder::BindDescriptorHeap`（Graphics + Compute）接口
- [x] 8.5 三后端 stub（`CreateDescriptorHeap` 返回 nullptr / `BindDescriptorHeap` 空实现）
- [ ] 8.6 tier2 heap 实现：Vulkan（`VK_EXT_descriptor_heap`）——拆独立 change
- [ ] 8.7 tier2 heap 实现：DX12（native `ID3D12DescriptorHeap` + SM6.6）——拆独立 change
- [ ] 8.8 tier2 heap 实现：Metal（argument buffer）——拆独立 change
