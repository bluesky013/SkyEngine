## Why

`ResourceGroup::CreateEncoder()`（`aurora-descriptor-encoder` 落地）是 per-set 的：一个 encoder 绑定一个 descriptor set，`End()` 只提交那一个 set。但真实的渲染帧需要**跨多个 set 的批量更新**（Global + Pass + Batch 各自更新），且多帧 in-flight 下每帧重写 descriptor set 会暴露覆盖 hazard：

- **Vulkan**：每个 RG 当前只有一个 `VkDescriptorSet`，每帧 `vkUpdateDescriptorSetWithTemplate` 重写会覆盖 GPU 仍在读的上一帧描述符（`aurora-descriptor-encoder` 只解决了 DX12 的 shader-visible 问题，Vulkan 侧单 set 隐患仍在）。
- **DX12**：shader-visible ring 已解决，但跨 set 更新没有一个统一的「帧级提交点」。

不同 tier 的更新频率/语义不同，需要分两种生命周期管理：

1. **Global（set 0）+ Pass（set 1）**：每帧/每 pass 写一次，内容帧内稳定 → 适合「每次更新重新申请一个新的 descriptor set」的 **transient** 模式，天然规避 in-place 重写 hazard。
2. **Batch（set 2）**：descriptor 本身稳定（dynamic UBO `offset=0, range=blockSize`），但每帧 pack constant buffer 内容一直在变 → 适合 **cached** 模式（稳定 descriptor + 每帧 pack buffer 池化）。

## What Changes

- 新增 frame-scoped **`DescriptorBatch`**：跨 RG 累积写入，`Flush()` 一帧一次（Vulkan 单次 `vkUpdateDescriptorSets` / DX12 no-op）
- **延迟释放（deferred release）机制**：被替换的 descriptor set / buffer / image 挂 retire 队列，等 in-flight 帧完成再归还，不立即释放
- **Global/Pass 持久化**：默认写一次长期复用（低更新频率）；偶发更新时重新申请新 set + 旧 set 延迟释放
- **Batch 尽可能持久化**：set 对象复用；应对「每帧 pack 可能拿到不同 buffer」和「纹理 streaming upgrade/downgrade」——内容变化时更新 binding + 延迟释放旧引用，优先保持 pack buffer handle 稳定
- **in-flight 隔离**：batch 累加缓冲 / descriptor pool（含 retire 队列）/ DX12 ring / pack buffer 都按 in-flight frame 分帧轮换
- `DeviceFrameContext` 持有 per-inflight-frame 的 batch + pool + retire 队列，`BeginFrame` 切帧 + 复用

## Capabilities

### New Capabilities

- `aurora-frame-descriptor-batch`: frame-scoped 跨 set 批量更新 + transient/cached 两种 set 生命周期 + per-inflight-frame 资源隔离

### Modified Capabilities

- `aurora-resource-binding`: 「Encoder::BindResourceGroup」绑定的是「当前帧的 set 视图」（transient 模式下每次更新换新 set，cached 模式稳定 set）

## Impact

- **接口层**：新增 `aurora/rhi/DescriptorBatch.h`；`Device::CreateDescriptorBatch()`；`DeviceFrameContext` 增 per-inflight-frame 状态
- **Vulkan**：`VulkanDescriptorBatch`（累积 `VkWriteDescriptorSet` + 单次 `vkUpdateDescriptorSets`）+ per-inflight-frame `VkDescriptorPool`（transient 模式）+ `vkResetDescriptorPool`
- **DX12**：`D3D12DescriptorBatch`（thin，写走既有 encoder + shader-visible ring）
- **Metal**：stub + TODO
- **测试**：batch 跨 set 累积 + Flush；transient set 轮换（多帧不覆盖）；cached batch 稳定 descriptor
- **依赖**：`aurora-dynamic-ubo-pack` 的 `BatchAllocator`（packed pool）作为 cached 模式的 pack buffer 来源
