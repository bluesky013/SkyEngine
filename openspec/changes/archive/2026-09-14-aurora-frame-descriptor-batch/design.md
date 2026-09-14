## Context

`aurora-descriptor-encoder` 落地了 per-set 的 `DescriptorEncoder`（Vulkan template + DX12 staging/ring）。两个待解决：

1. **无跨 set 批量**：真实帧要更新多个 set（Global/Pass/Batch），没有统一的帧级提交点。
2. **Vulkan 单 set 覆盖 hazard**：每个 RG 只有一个 `VkDescriptorSet`，in-place 重写会覆盖 GPU 仍在读的描述符。

约束：3 后端；命名空间 `sky::aurora`；`DeviceFrameContext` 已有 `mInflightNum` + `mFrameIndex` + per-inflight-frame command buffer。

## Goals / Non-Goals

**Goals:**

- frame-scoped `DescriptorBatch`：跨 RG 累积，一帧一次 `Flush()`
- **持久化 + 延迟释放**：descriptor set 默认持久，内容变化时才换新 set，旧 set 等 in-flight 帧完成再归还
- Global/Pass（持久化、低更新频率）与 Batch（尽可能持久化、应对 buffer/纹理变化）两种语义
- in-flight 隔离：pool / ring / pack buffer 按 in-flight frame 轮换，帧退役复用

**Non-Goals:**

- 不实现 bindless descriptor heap（tier2）
- 不改 `DescriptorEncoder`（per-set template 路径保留）
- 不做 descriptor 内容哈希去重（纯优化）
- Metal 实现（stub）

## Decisions

### 决策 1：frame-scoped `DescriptorBatch` 接口

```cpp
// aurora/rhi/DescriptorBatch.h（接口层）
class DescriptorBatch {
public:
    virtual ~DescriptorBatch() = default;
    virtual void WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer,
                             uint64_t offset, uint64_t range, uint32_t arrayElement = 0) = 0;
    virtual void WriteImage(ResourceGroup *group, uint32_t binding, Image *image,
                            ImageLayout layout, uint32_t arrayElement = 0) = 0;
    virtual void WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler,
                              uint32_t arrayElement = 0) = 0;
    virtual void Flush() = 0;
    virtual void Reset() = 0;
};
```

`Device::CreateDescriptorBatch()` 创建；`DeviceFrameContext` 按 in-flight frame 各持一个，`BeginFrame` 切当前 + `Reset()`。

### 决策 2：延迟释放（deferred release）机制

descriptor set（及其 buffer/image 引用）被替换时**不立即释放**，而是挂到当前帧的 **retire 队列**，等该帧 fence 完成（`inflightNum` 帧后）再归还池/释放。

- 每 in-flight frame 一个 retire 队列；帧提交时把「本帧被替换的 set」登记；帧退役时回收队列里的 set。
- 这是 in-place 重写 hazard 的统一解：被替换的 set 在 GPU 用完前一直存活。

**Why:** 「持久化 + 延迟释放」是比「每帧重申请」或「N 份 set 版本化」更贴合实际使用频率的方案——低频更新几乎零开销，高频更新也只在实际变化时付出分配成本。

### 决策 3：Global/Pass 持久化 + 罕见更新换新 + 延迟释放

Global（set 0）/ Pass（set 1）**持久化**：默认持一个 set，写一次不重写（低更新频率）。当需要更新（render 开关变化触发 `OnPipelineChanged`，如开关 shadow / IBL）时：

1. 从 descriptor pool 重新申请一个新 set（Vulkan `vkAllocateDescriptorSets`）
2. 写入 + 绑定新 set
3. 旧 set 挂 retire 队列，`inflightNum` 帧后归还（`vkFreeDescriptorSets` 或整池 reset）

**Why:** 更新频率低，默认路径是「写一次、长期复用」；偶发更新走换新 + 延迟释放，避免 in-place hazard，且不常发生所以成本可忽略。

### 决策 4：Batch 尽可能持久化 + cached 内容 + 变化时更新

Batch（set 2）**尽可能持久化**：descriptor set 对象复用，不每帧重分配。但内容可能变化：

- **每帧 pack UBO 可能拿到不同 buffer**（packed pool 按 in-flight frame 轮换，buffer handle 每帧可能变）
- **纹理 streaming upgrade/downgrade**（换 mip / 换 image view）

管理方式：

- descriptor set 持久；**内容变化时更新对应 binding**，被替换的 buffer/image 引用走 retire 队列延迟释放。
- 为降低「每帧 buffer 变化」带来的 descriptor 重写：**优先让 pack buffer handle 稳定**——用单一持久 buffer 内部分段 ring（`BatchAllocator` 线性分配在同一 buffer 上），dynamic offset 选数据；只有 buffer 真的换掉（扩容/换池）时才更新 descriptor binding。
- streaming upgrade/downgrade：只在 mip 级别 / view 真正变化时更新 image binding，其余时间 cached 不动。

**Why:** batch 的「变化」尽量收敛到数据层（pack buffer 内容 + dynamic offset），descriptor 层尽量稳定；真正变化时再更新 binding + 延迟释放旧引用。

### 决策 5：cache key 下沉 Vulkan + bufferid/viewid/samplerid

cache key 整套逻辑下沉 Vulkan 内部，**不加到接口层**：

- `VulkanDevice` 持单调计数器 `std::atomic<uint64_t>` 发号；`VulkanBuffer` / `VulkanImage` / `VulkanSampler` 包装对象持单调 **ID**（创建时分配，永不复用）。
- `Image::GetView(ViewDesc)` 在 Vulkan 侧按 view 缓存，每个 view 发单调 **viewid**。
- cached-content cache key：buffer `(binding, bufferId)`、image `(binding, viewid)`、sampler `(binding, samplerId)`。

接口层不暴露 resource ID、不感知 cache key；ABA（native handle 复用 / 指针复用）由 Vulkan 侧单调 ID 规避。

| 层 | key | 后端 |
|---|---|---|
| 1. layout / template 去重 | `Hash([ (binding, type, count, stage) ... ])` | Vulkan |
| 2. set 复用（pool/retire） | `VkDescriptorSetLayout` | Vulkan |
| 3. cached 内容（batch） | `(binding, bufferId/viewid/samplerId)` | Vulkan |

**Why:** cache 就是 Vulkan descriptor set 模式的产物，资源身份也随之下沉 Vulkan，接口层不背这个负担。

### 决策 5b：texture streaming 对接（GetView + ViewDesc，无 View 类）

- **residency/sparse（B）预留**：因 sparse 支持参差（尤其移动端），长期方向，本 change 不实现，只预留。
- **view-swap 走 `GetView`，不暴露 `View` 类**：view 是 Image 的子资源引用，`Image::GetView(ViewDesc)` 内部按 `ViewDesc` 缓存 native view；`ViewDesc = {baseMip, mipCount, baseLayer, layerCount, usage}` 是纯值，无独立 ID/引用计数/延迟释放。
- **cached-content key 扩展**：image binding 用 `(binding, viewid)`——`GetView(ViewDesc)` 发单调 viewid，判「view 变没变」直接用 viewid，无需 imageId + ViewDesc 复合。
- **流程**：streaming 预算/可见性决定新 mip 范围 → 材质 binding 更新 `ViewDesc` → `GetView` 返回新 viewid → cached key 变 → 重写 binding；旧 view 由 Image 的 view 缓存自行管理。

### 决策 6：per-backend `DescriptorBatch` + in-flight 隔离

- **Vulkan** `VulkanDescriptorBatch`：`Write*` 累积 `VkWriteDescriptorSet`（`dstSet` = 各 group 当前 set）+ info；`Flush()` 单次 `vkUpdateDescriptorSets`。
- **DX12** `D3D12DescriptorBatch`：`Write*` 复用 `D3D12DescriptorEncoder`（即时写 CPU staging）；`Flush()`/`Reset()` no-op（copy 在 bind）。
- **Metal** `MetalDescriptorBatch`：stub。

in-flight 隔离资源：batch 累加缓冲、Vulkan descriptor pool（含 retire 队列）、DX12 ring、pack buffer（packed pool）都按 `inflightNum` 分帧轮换，帧退役复用。

## Risks / Trade-offs

- **retire 队列的时序** → 缓解：帧退役（fence 完成）驱动回收，与 command buffer 复用同一 fence 语义
- **Batch 每帧 buffer 变化仍可能触发 descriptor 更新** → 缓解：优先单一持久 pack buffer 内部 ring（handle 稳定）；buffer 换池才更新 binding
- **streaming 的 upgrade/downgrade 粒度** → 缓解：mip 级别变化才换 image view，其余 cached
- **pool 容量预估** → 缓解：Global/Pass 更新罕见，pool 单调分配 + 整池 reset；不足时扩容

## Migration Plan

1. 接口层：`DescriptorBatch.h` + `Device::CreateDescriptorBatch()` + `DeviceFrameContext` 增 batch/pool/retire 队列
2. Vulkan：`VulkanDescriptorBatch` + descriptor pool（含 retire 队列）+ Global/Pass 换新 + Batch cached
3. DX12：`D3D12DescriptorBatch`（thin，复用 encoder + ring）
4. Metal：stub
5. 迁移调用方 + 测试
6. 文档

## Open Questions

- **Batch 的 pack buffer 是否强制单一持久 buffer（handle 稳定）？** 倾向：是，`BatchAllocator` 已经在单一 buffer 上线性分配，保持 handle 稳定；pool 扩容换 buffer 时才更新 descriptor。
- **retire 粒度**：per-set free（`VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT`）vs 整池 reset。倾向：Global/Pass 用整池 reset（简单），Batch 持久 set 不重分配。
- **streaming 是否本 change 范围内？** 倾向：只定义「变化时更新 binding + 延迟释放」契约，实际 streaming 逻辑随材质系统落地。
