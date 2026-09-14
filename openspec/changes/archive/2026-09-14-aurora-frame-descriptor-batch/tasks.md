## 1. 接口层

- [x] 1.1 新增 `aurora/rhi/DescriptorBatch.h`：抽象类 `WriteBuffer` / `WriteImage` / `WriteSampler` + `Flush()` / `Reset()`
- [x] 1.2 `aurora/rhi/Device.h`：加 `virtual DescriptorBatch *CreateDescriptorBatch() = 0`
- [x] 1.3 `DeviceFrameContext` per-inflight-frame batch + retire 队列 —— **拆分到后续**（依赖 renderer fence，见文末「后续」）
- [x] 1.4 接口层不新增 resource ID（cache key 下沉 Vulkan，见 3.5）

## 2. 延迟释放（deferred release）机制

- [x] 2.1 retire 语义 —— **拆分到后续**（依赖 renderer fence）
- [x] 2.2 per-inflight-frame retire 队列 —— **拆分到后续**（依赖 renderer fence）

## 3. Vulkan 后端

- [x] 3.1 新增 `VulkanDescriptorBatch.h/.cpp`：`Write*` 累积 `VkWriteDescriptorSet`（`dstSet` = group 当前 set）+ info；`Flush()` 单次 `vkUpdateDescriptorSets`；`Reset()` 清空
- [x] 3.2 descriptor pool + 换新 set —— **拆分到后续**（依赖 retire 队列 + OnPipelineChanged）
- [x] 3.3 `VulkanResourceGroup::ReplaceSet` —— **拆分到后续**
- [x] 3.4 `BindResourceGroup` 当前 set 视图 —— **拆分到后续**
- [x] 3.5 Vulkan 侧 resource ID：`VulkanDevice` 单调发号；`VulkanBuffer`/`VulkanImage`/`VulkanSampler` 包装对象持 ID（viewid 随 `GetView(ViewDesc)` API 后续落地，暂缓）

## 4. DX12 后端

- [x] 4.1 新增 `D3D12DescriptorBatch.h/.cpp`：thin，`Write*` 复用 `D3D12DescriptorEncoder`（即时写 CPU staging）；`Flush()`/`Reset()` no-op（copy 在 bind）

## 5. Metal 后端

- [x] 5.1 新增 `MetalDescriptorBatch.h`：stub + TODO（随 `aurora-resource-group Metal phase`）

## 6. Batch cached 模式

- [x] 6.1 Batch cached 模式 —— **拆分到后续**（依赖 retire 队列）
- [x] 6.2 pack buffer handle 稳定 —— **拆分到后续**
- [x] 6.3 cached 契约文档化 —— **拆分到后续**

## 7. 迁移 + 测试

- [x] 7.1 迁移 `GlobalRenderResources` / `PipelinePass` —— **拆分到后续**
- [x] 7.2 新增 batch 跨 set 累积测试：`WriteBuffer` 到两个 group + `Flush()` 单次提交
- [x] 7.3 延迟释放测试 —— **拆分到后续**
- [x] 7.4 Global/Pass 持久化测试 —— **拆分到后续**
- [x] 7.5 batch cached 测试 —— **拆分到后续**

## 8. 收尾 / 文档

- [x] 8.1 更新 `engine/aurora/AGENTS.md`：记录 frame-scoped `DescriptorBatch`、持久化 + 延迟释放、Global/Pass 持久化、Batch cached
- [x] 8.2 跑 `AuroraRHITest` / `AuroraPipelineTest` 全绿（至少 Vulkan）
- [ ] 8.3 archive 本 change（待用户确认）

## 后续（拆分到 `aurora-renderer` 落地后）

以下依赖 renderer 的 **fence 生命周期**（retire 回收）和 **OnPipelineChanged 事件**（transient 触发），当前均未接线，拆到后续 change：

- retire 队列 + 帧退役回收（`vkFreeDescriptorSets` / 整池 reset）
- Global/Pass transient 换新 set（OnPipelineChanged 触发）+ 延迟释放
- Batch cached 模式（pack buffer handle 稳定 + streaming 换 ViewDesc）
- viewid（依赖 `GetView(ViewDesc)` API）
- 对应测试（延迟释放 / 持久化 / cached）与 `GlobalRenderResources`/`PipelinePass` 迁移
