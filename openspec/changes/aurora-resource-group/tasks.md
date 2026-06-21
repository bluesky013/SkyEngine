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

- [ ] 3.1–3.8 全部留下次实施（接口 stub 已加，编译路径已通过；本轮未做实质实现）

## 4. Metal 后端

- [ ] 4.1–4.7 全部留下次实施（接口 stub 已加，编译通过）

## 5. GLES 后端

- [ ] 5.1–5.5 全部留下次实施（接口 stub 已加，条件编译路径预期通过）

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
- [ ] 6.4 4 后端 fixture 复用：本轮只 Vulkan

## 7. 收尾 / 文档

- [ ] 7.1 AGENTS.md "Resource binding 模型" 章节：留下次（待 4 后端实现完成）
- [x] 7.2 `AuroraTest --gtest_filter=ResourceGroup*` 7/7 PASSED；全集 102/102 PASSED 无回归
- [x] 7.3 Vulkan validation 不报新 warning
- [ ] 7.4 archive：留待 DX12/Metal/GLES 真正实现后再 archive
