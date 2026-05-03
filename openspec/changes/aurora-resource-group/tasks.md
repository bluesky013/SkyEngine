## 1. 接口层

- [ ] 1.1 修改 `aurora/rhi/ResourceGroup.h`：完整重写
  - [ ] 1.1.1 `ResourceGroupLayout::BindingDesc` + `Descriptor`
  - [ ] 1.1.2 `ResourceGroup::Descriptor` 含 layout 指针
  - [ ] 1.1.3 `ResourceUpdateInfo` + `ResourceWriteKind` 枚举 + 联合体
  - [ ] 1.1.4 `ResourceGroup::Update(writes)` 虚接口
- [ ] 1.2 新增 `aurora/rhi/PipelineLayout.h`：`PipelineLayout` 类 + `Descriptor`
- [ ] 1.3 修改 `aurora/rhi/PipelineState.h`：`GraphicsPipeline::Descriptor` / `ComputePipeline::Descriptor` 加 `PipelineLayout *layout` 必填
- [ ] 1.4 修改 `aurora/rhi/Device.h`：
  - [ ] 1.4.1 重命名 `CreateSampler(ResourceGroup::Descriptor)` → `CreateResourceGroup`（如 `aurora-quick-fixes` 已先行 land 则跳过）
  - [ ] 1.4.2 加 `CreateResourceGroupLayout` 与 `CreatePipelineLayout`
- [ ] 1.5 修改 `aurora/rhi/Encoder.h`：
  - [ ] 1.5.1 `BindResourceGroup` 加 `numDynamicOffsets` / `dynamicOffsets` 参数
  - [ ] 1.5.2 加 `PushConstants` 接口（Graphics 含 stages，Compute 不含）

## 2. Vulkan 后端

- [ ] 2.1 新增 `VulkanResourceGroupLayout.h/.cpp`：`vkCreateDescriptorSetLayout`
- [ ] 2.2 新增 `VulkanPipelineLayout.h/.cpp`：聚合多 set + push constants → `vkCreatePipelineLayout`
- [ ] 2.3 新增 `VulkanDescriptorPool.h/.cpp`：device 内部持多 pool；按 layout type 计数预留容量；满了开新 pool
- [ ] 2.4 新增 `VulkanResourceGroup.h/.cpp`：`vkAllocateDescriptorSets` + `Update` 用 `vkUpdateDescriptorSets`
- [ ] 2.5 修改 `VulkanGraphicsPipeline` / `VulkanComputePipeline`：从 `Descriptor::layout` 取 `VkPipelineLayout`
- [ ] 2.6 实现 `VulkanGraphicsEncoder::BindResourceGroup` / `VulkanComputeEncoder::BindResourceGroup`：`vkCmdBindDescriptorSets`，传 dynamic offsets
- [ ] 2.7 实现 `VulkanGraphicsEncoder::PushConstants` / `VulkanComputeEncoder::PushConstants`：`vkCmdPushConstants`
- [ ] 2.8 删除 `VulkanDevice::CreateSampler(ResourceGroup::Descriptor)` 旧 stub override

## 3. DX12 后端

- [ ] 3.1 新增 `D3D12ResourceGroupLayout.h/.cpp`：把 BindingDesc 翻成一段 root parameter 描述（descriptor table ranges）
- [ ] 3.2 完整化 `D3D12RootSignature`（当前是 stub）：从 PipelineLayout 构造完整 root signature；预留 set 数 ≤ 4 + push constants root constants 槽
- [ ] 3.3 新增 `D3D12PipelineLayout.h/.cpp`：包装 RootSignature
- [ ] 3.4 新增 `D3D12DescriptorHeapPool`：device 维护 CBV/SRV/UAV 大 heap + SAMPLER 大 heap；按 group 分配连续 slot；满了开子 heap
- [ ] 3.5 新增 `D3D12ResourceGroup.h/.cpp`：持有 heap 中的 CPU/GPU handle 起点；Update 用 `CopyDescriptors`
- [ ] 3.6 完整化 `D3D12GraphicsPipeline` / `D3D12ComputePipeline`（当前 stub 返回 nullptr）：从 `Descriptor::layout` 取 RootSignature；走 `CreateGraphicsPipelineState` / `CreateComputePipelineState`
- [ ] 3.7 实现 `D3D12GraphicsEncoder::BindResourceGroup`：`SetDescriptorHeaps`（按 device heap）+ `SetGraphicsRootDescriptorTable(set 索引对应的 root param, gpuHandle)`；dynamic offsets 在 D3D12 上需把 dynamic CBV 改用 root CBV slot
- [ ] 3.8 实现 `D3D12*Encoder::PushConstants`：`SetGraphicsRoot32BitConstants`

## 4. Metal 后端

- [ ] 4.1 新增 `MetalResourceGroupLayout.h/.mm`：构造 `MTLArgumentEncoder` 描述
- [ ] 4.2 新增 `MetalPipelineLayout.h/.mm`：维护 set→argument buffer slot 表 + push constants slot（约定 slot 30）
- [ ] 4.3 新增 `MetalArgumentBufferPool`：device 内部 `MTLBuffer` 池
- [ ] 4.4 新增 `MetalResourceGroup.h/.mm`：持有 argument buffer 段；Update 用 `MTLArgumentEncoder` 编码资源指针
- [ ] 4.5 修改 `MetalGraphicsPipeline` / `MetalComputePipeline`：从 layout 取 slot 表
- [ ] 4.6 实现 `MetalGraphicsEncoder::BindResourceGroup`：`setVertexBuffer:offset:atIndex:` + `setFragmentBuffer:` + `useResource:` 标 residency
- [ ] 4.7 实现 `MetalGraphicsEncoder::PushConstants` 用 `setVertexBytes:`+`setFragmentBytes:`；`MetalComputeEncoder::PushConstants` 用 `setBytes:length:atIndex:`

## 5. GLES 后端

- [ ] 5.1 新增 `GLESResourceGroupLayout.h/.cpp`：保存 BindingDesc 数组
- [ ] 5.2 新增 `GLESPipelineLayout.h/.cpp`：跨 set 校验 binding 唯一性；构造 binding→type/count 平表
- [ ] 5.3 新增 `GLESResourceGroup.h/.cpp`：保存 (binding, arrayElement) → resource 映射表；Update 仅写入映射，BindResourceGroup 时再 flush
- [ ] 5.4 实现 `GLESGraphicsEncoder::BindResourceGroup`：按映射表录制延迟命令（GLES 是软命令列表）：`glBindBufferBase` / `glBindBufferRange`（含 dynamic offset）/ `glBindTextureUnit` / `glBindSampler`
- [ ] 5.5 实现 `GLESGraphicsEncoder::PushConstants`：用预留 UBO（slot 0）模拟，写入 `glBufferSubData`

## 6. 测试

- [ ] 6.1 扩展 `AuroraTestHelper`：`MakeUniformBuffer`、`MakeSampledTexture`、`MakeLayout`、`MakePipelineLayout`、`MakeResourceGroup` 工具
- [ ] 6.2 新增 `ResourceGroupTest.cpp`：
  - [ ] 6.2.1 `CreateLayoutAndGroup`：基本创建路径
  - [ ] 6.2.2 `LayoutDuplicateBindingRejected`：重复 binding 返回 nullptr
  - [ ] 6.2.3 `BindAndDrawSingleSet`：1 set + UB + 采样纹理；Draw 后 readback 验证像素
  - [ ] 6.2.4 `BindAndDrawMultiSet`：2 sets；shader 同时引用
  - [ ] 6.2.5 `DynamicUniformOffset`：同 group 两次 BindResourceGroup 不同 offset，结果不同
  - [ ] 6.2.6 `ArrayBinding`：count=8 sampled image array；shader 索引 arr[3]
  - [ ] 6.2.7 `PushConstantsRoundTrip`：写 16 字节，shader 读出
  - [ ] 6.2.8 `LayoutGroupsExceedsFourRejected`：5 个 set 拒绝
  - [ ] 6.2.9 `MismatchedUpdateKindAsserts`：debug build assert 触发
- [ ] 6.3 修改 `EncoderTest::CreateMinimalVulkanGraphicsPipeline` 工具：补 PipelineLayout（即使空 layout 也要创建一个）
- [ ] 6.4 4 后端各跑一份 `ResourceGroupTest`（通过 fixture 复用）

## 7. 收尾 / 文档

- [ ] 7.1 在 `engine/aurora/AGENTS.md` 加一节"Resource binding 模型"：layout / group / pipeline-layout 三段式 + 4 后端 binding index 一致性约定
- [ ] 7.2 跑 `AuroraTest --gtest_filter=ResourceGroup*` 全绿（至少 Vulkan + 第二后端）
- [ ] 7.3 Vulkan validation / D3D12 debug layer / Metal validation 不报新 warning
- [ ] 7.4 archive：`openspec archive aurora-resource-group`
