# aurora-frame-descriptor-batch Specification

## Purpose

定义 frame-scoped 的跨 set descriptor 批量更新，以及「持久化 + 延迟释放」的 descriptor set 生命周期模型：Global/Pass 持久化（罕见更新换新），Batch 尽可能持久化（应对 pack buffer / streaming 变化），含 per-inflight-frame 资源隔离。

## ADDED Requirements

### Requirement: DescriptorBatch 接口

`DescriptorBatch` SHALL 是接口层抽象，跨多个 `ResourceGroup` 累积写入，`Flush()` 一帧提交一次，`Reset()` 帧末复用：

- `WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0)`
- `WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0)`
- `WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0)`
- `Flush()` / `Reset()`

`Device::CreateDescriptorBatch()` SHALL 创建 batch。`DeviceFrameContext` SHALL 按 `inflightNum` 持有 batch，`BeginFrame` 切当前并 `Reset()`。

#### Scenario: 跨 set 累积单次提交

- **WHEN** 对 set 0 / set 1 的两个 group 各自 `WriteBuffer` 后 `Flush()`
- **THEN** Vulkan 用一次 `vkUpdateDescriptorSets` 写入两个 set；后续 Bind + Draw 各自采样正确

### Requirement: 延迟释放（deferred release）

被替换的 descriptor set（及其 buffer/image 引用）SHALL **不立即释放**，而是登记到当前 in-flight frame 的 retire 队列，等该帧 fence 完成（`inflightNum` 帧后）再归还 pool / 释放。

#### Scenario: 旧 set 延迟释放

- **WHEN** 帧 N 更新某 RG（换新 set），旧 set 登记到帧 N 的 retire 队列
- **THEN** 旧 set 在帧 N+inflightNum 退役前保持存活；退役后归还 pool

### Requirement: Global/Pass 持久化 + 罕见更新换新

Global（set 0）/ Pass（set 1）SHALL **持久化**：默认持一个 set 写一次长期复用（低更新频率）。需要更新时 SHALL 重新申请一个新 set，绑定新 set，旧 set 走 retire 队列延迟释放。

#### Scenario: 默认不重分配

- **WHEN** 连续多帧 Global set 内容不变
- **THEN** 不重新分配 set，复用同一 set（无 per-frame 分配）

#### Scenario: 偶发更新换新 + 延迟释放

- **WHEN** 场景/视图变化触发 Global 更新
- **THEN** 申请新 set 并绑定；旧 set 登记 retire 队列，`inflightNum` 帧后归还

### Requirement: Batch 尽可能持久化 + cached 内容

Batch（set 2）SHALL **尽可能持久化**：descriptor set 对象复用，不每帧重分配。内容变化时（每帧 pack 可能拿到不同 buffer、纹理 streaming upgrade/downgrade）SHALL 更新对应 binding，被替换的 buffer/image 引用走 retire 队列延迟释放。

为降低每帧 buffer 变化带来的 descriptor 重写，SHALL 优先保持 pack buffer handle 稳定（单一持久 buffer 内部分段，dynamic offset 选数据）；只有 buffer 真正换掉（扩容/换池）时才更新 descriptor binding。

#### Scenario: pack buffer handle 稳定

- **WHEN** pack buffer 是单一持久 buffer，帧间只改 dynamic offset
- **THEN** descriptor binding 不更新（handle 稳定），无重写

#### Scenario: buffer 换池时更新 binding

- **WHEN** pack buffer 扩容/换池导致 buffer handle 变化
- **THEN** 更新 descriptor binding 指向新 buffer；旧 buffer 引用延迟释放

#### Scenario: streaming upgrade/downgrade

- **WHEN** 材质纹理 streaming 换 mip 级别（换 image view）
- **THEN** 仅更新 image binding 指向新 view；旧 view 引用延迟释放；其余 binding cached 不动

### Requirement: cache key 下沉 Vulkan（bufferid/viewid/samplerid）

cached 内容的 cache key 整套下沉 Vulkan 内部：`VulkanBuffer` / `VulkanImage` / `VulkanSampler` 包装对象持单调 ID（`VulkanDevice` 创建时发号，永不复用），view 由 `GetView(ViewDesc)` 发单调 viewid。cached-content key = `(binding, bufferId / viewid / samplerId)`，MUST 不用原生 handle（会被驱动回收复用）或对象指针（地址可能复用）。接口层不暴露 resource ID、不感知 cache key。

#### Scenario: 对象复用不误判

- **WHEN** 旧 buffer 销毁后新 buffer 创建（Vulkan 侧 ID 单调递增，二者 ID 必然不同）
- **THEN** cached 内容比较 ID 判为「已变」，触发 descriptor 重写，无误判

### Requirement: in-flight 资源隔离

`DescriptorBatch` 累加缓冲、Vulkan descriptor pool（含 retire 队列）、DX12 shader-visible ring、Batch pack buffer SHALL 都按 `inflightNum` 分帧轮换，帧退役（fence 完成）后复用。

#### Scenario: 帧间资源不冲突

- **WHEN** `inflightNum=2`，帧 0 与帧 1 各自持有独立的 batch/pool/ring 段/pack buffer
- **THEN** 帧 1 的写入不覆盖帧 0 仍在 GPU 使用的资源
