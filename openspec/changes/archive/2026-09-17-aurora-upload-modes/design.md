## Context

`aurora-upload` 在 RHI 层已定义两种上传模式：

- **异步**：`Queue::UploadBuffer/UploadImage` 提交到 transfer/upload 队列，返回 `TransferTaskHandle`（`uint32_t`），`Queue::Wait(handle)` 阻塞、`Queue::HasComplete(handle)` 查询；调用方负责同步。
- **当帧 inline**：`StagingBufferAllocator` 每帧 bump 分配 + `BlitEncoder::CopyBuffer` 内联到当前帧 command buffer。

resource 层现状（`engine/aurora/core/include/aurora/resource/`）：

| 资源 | 上传实现 | 内存 | 问题 |
|---|---|---|---|
| `StaticBuffer::Upload` | `Queue::UploadBuffer` | GPU_ONLY | **丢弃 handle，从不 Wait** → 在途销毁校验错误 |
| `Texture::UploadImage` | `Queue::UploadImage` | GPU_ONLY | 同上 |
| `DynamicBuffer::Upload/Write` | 持久映射 memcpy + ring | CPU_TO_GPU | 手动 `AdvanceFrame()`，与 `DeviceFrameContext` 无显式对齐 |
| `TransientBuffer::Upload` | 持久映射 memcpy | CPU_TO_GPU | 同上（单 buffer stub） |

`DeviceFrameContext`（`aurora/rdg/RenderDeviceExclusive.h`）：`mFrameIndex` + `mFences[mInflightNum]` + `GetFrameFence()`；`BeginFrame()` 对当前槽 fence `Wait()+Reset()`（保证该槽可复用），`EndFrame()` 重置 allocator + `++mFrameIndex`。

约束：`namespace sky::aurora`；不新增 RHI 接口（复用 `Queue::Wait/HasComplete`）；行为语义除「异步完成同步」外不变；命名遵守 coding rules（无尾下划线、注释只解释 why）。

## Goals / Non-Goals

**Goals:**

- 明确 resource 层两种上传模式：**异步（transfer queue）** 与 **当帧直写（CPU_TO_GPU 持久映射 + frame context ring）**。
- 修复异步完成缺口：`StaticBuffer`/`Texture` 追踪 pending handle + queue，`WaitUploadComplete()`/`IsUploadComplete()`，`Release()` 前等待。
- 明确当帧直写 ring 与 `DeviceFrameContext` 的 in-flight 帧对齐契约。
- 测试验证完成语义 + 消除在途销毁校验错误。

**Non-Goals:**

- 不改 RHI 接口（`Queue::UploadBuffer/UploadImage`、`StagingBufferAllocator` 契约不变）。
- 不把 `DynamicBuffer` 切到 `StagingBufferAllocator + CopyBuffer` inline 路径——持久映射直写对 CPU_TO_GPU 目标更简单高效，inline staging 留给「GPU_ONLY 目标 + 每帧上传」的场景（后续 change）。
- 不给 texture 加当帧直写模式（v1 纹理一律 GPU_ONLY，异步 only）。
- 不把 `DeviceFrameContext` 指针注入 resource 层（避免 core→rdg 依赖）；对齐契约靠 frame driver 锁步驱动。

## Decisions

### D1: 两种上传模式（异步 vs 当帧 staging），不按方法名分

两种上传语义，不引入 `UploadAsync`/`UploadImmediate` 两套方法：

| 模式 | 载体 | 机制 | 同步 |
|---|---|---|---|
| 异步（transfer queue） | `StaticBuffer`、`Texture` | staging → transfer 队列 copy | `TransferTaskHandle` + `Wait` |
| 当帧 staging（frame context） | `FrameStagingBuffer` | `StagingBufferAllocator` 每帧 bump + 内联 `BlitEncoder::CopyBuffer` | frame context in-flight ring |

- **理由**：异步上传针对「上传一次/低频、GPU_ONLY」的资源（`StaticBuffer`/`Texture`）；当帧 staging 上传针对「每帧写入 GPU_ONLY 目标」的场景，用 `StagingBufferAllocator` 的每帧 ring（随 `DeviceFrameContext` 驱动）避免每帧建临时 staging。
- **注意**：`DynamicBuffer`/`TransientBuffer` 的 CPU_TO_GPU 持久映射直写是**第三种机制**（无 copy，非「上传」），不属于上述两种上传模式；其 ring 锁步契约见 D4。
- **备选**：`UploadAsync`/`UploadImmediate` 两套方法 —— 被否，载体（资源类型 vs uploader）已决定唯一语义，多一套方法只是噪音。

### D2: 异步上传追踪 handle，析构前等待

`StaticBuffer` / `Texture` 在 `Upload`/`UploadImage` 里保存 `pendingHandle`（`TransferTaskHandle`）+ `pendingQueue`（`Queue*`），并暴露：

```cpp
bool IsUploadComplete() const
{
    return pendingQueue == nullptr || pendingQueue->HasComplete(pendingHandle);
}
void WaitUploadComplete()
{
    if (pendingQueue != nullptr) {
        pendingQueue->Wait(pendingHandle);
        pendingQueue = nullptr;
    }
}
```

底层 buffer/image 由 `BufferPtr`/`ImagePtr` 成员持有，其析构发生在 `StaticBuffer`/`Texture` 析构时。故析构函数（在成员析构、释放底层 buffer/image 之前）SHALL 调 `WaitUploadComplete()`：

```cpp
~StaticBuffer() override { WaitUploadComplete(); }   // Texture 同理
```

- **理由**：`TransferTaskHandle` 是 per-queue 的（须用提交它的同一 `Queue*` 去 `Wait`），所以必须同时记住 handle 和 queue；析构前等待消除了 `vkDestroyBuffer`/`vkDestroyImage`「在途使用」校验错误。queue 由 `device` 持有，生命周期长于资源，裸指针安全。
- **注意（实现细节）**：`RenderResource::Release()` 当前是**未接入析构的死代码**（`~RenderResource() = default` 不调它，底层资源由 `BufferPtr`/`ImagePtr` 成员析构释放）。因此完成同步必须放在析构函数里，而非 `Release()`；`Release()` 里也补了 `WaitUploadComplete()`（防御性，若未来接入析构则已正确）。
- **备选**：让 `Upload` 直接返回 handle 给调用方自管 —— 被否，破坏 `RenderResource::Upload` 统一 `bool` 契约，且把同步责任外推易遗漏（正是当前 bug 的成因）。
- **注意**：`WaitUploadComplete()` 会阻塞调用线程（transfer 队列），仅应在明确需要同步时（析构 / 消费前）调用；普通帧内路径不调它，异步语义不变。

### D3: 当帧 staging 上传 = FrameStagingBuffer（包装 StagingBufferAllocator）

当帧 staging 上传落地为 resource 层 `FrameStagingBuffer`（`aurora/resource/FrameStagingBuffer.h`，header-only），包装 RHI 的 `StagingBufferAllocator`：

```cpp
class FrameStagingBuffer {
public:
    bool Init(Device *dev, uint64_t segmentSize, uint32_t numFrames);
    bool Upload(Buffer *dst, const void *data, uint64_t size, uint64_t dstOffset = 0);
    void Flush(BlitEncoder &encoder);
    void Reset();
private:
    StagingBufferAllocator allocator;
    std::vector<PendingCopy> pending;  // {src, srcOffset, dst, dstOffset, size}
};
```

- `Upload`（host 侧，任意时机）：`allocator.Allocate(size, 1)` 拿当前帧 staging 槽 + `memcpy` + 排队 `PendingCopy`。
- `Flush`（渲染侧，COPYBLIT pass 内）：逐条 `encoder.CopyBuffer(src, dst, size, srcOffset, dstOffset)` 内联，清空队列。
- `Reset`（frame context hook）：`allocator.Reset()` 推进到下一 in-flight 段（每帧一次）。
- `numFrames` SHALL 等于 `DeviceFrameContext::inflightNum`；in-flight 安全由 `StagingBufferAllocator` 的 ring + frame context 的 fence 保证。

- **理由**：`StagingBufferAllocator`（RHI）已是 frame-context 驱动的 per-frame ring；resource 层只补「排队 + 内联 flush」的便利封装，把两阶段（host 写 / 渲染内联 copy）收进一个对象，对齐 `aurora-upload` 的「当帧 inline」模式。
- **备选**：让调用方直接用 `StagingBufferAllocator` + 手写 `CopyBuffer` —— 被否，每次都要手排 copy 队列，且「两阶段」语义散落。
- **注意**：v1 只做 buffer copy（`CopyBuffer`）；当帧 staging 纹理上传（`CopyBufferToImage`）留待后续。

### D4: DynamicBuffer/TransientBuffer 的 ring 与 frame context 锁步（持久映射，非上传模式）

`DynamicBuffer` 的 ring（`numFrames` 个 buffer + `current`）SHALL 与 `DeviceFrameContext` 对齐：

- `numFrames`（`Init` 的 `framesInFlight`）SHALL 等于 frame context 的 `inflightNum`；
- frame driver 每帧在 `DeviceFrameContext::BeginFrame()`（已对当前槽 fence `Wait+Reset`，保证槽可写）之后调 `DynamicBuffer::AdvanceFrame()`，使 `current` 与 `mFrameIndex` 同锁步。

持久映射直写的 in-flight 安全由 frame context 的 fence 保证，而非 buffer 自建同步。`AdvanceFrame()` 语义不变（纯槽位轮换），本 change 只把契约写清楚（注释 + spec），不改实现。

- **理由**：`DeviceFrameContext::BeginFrame()` 已经实现了「等待第 `mFrameIndex % inflightNum` 槽的 fence」，这正是 buffer ring 需要的同步；让两者锁步即可复用，避免 resource 层自建 fence。
- **注意**：`aurora-renderer`（top-level 渲染主循环）尚未落地，本 change 不强行接线；`AdvanceFrame()` 仍由调用方手动驱动，契约写进 spec 供 renderer 落地时遵守。

### D5: 不动 RHI 层

`aurora-upload` 的两种模式契约、`StagingBufferAllocator`、`Queue::Wait/HasComplete`、`BlitEncoder::CopyBuffer` 均不修改；本 change 只在 resource 层补齐完成语义、落地当帧 staging 上传、写清 ring 契约。

- **理由**：RHI 层契约已正确（异步 + 当帧 inline），缺口全在 resource 层对 handle 的忽略、对当帧 staging 的缺失、对 frame context 的隐式依赖。
- **备选**：改 `Queue::Upload*` 为同步 —— 被否，破坏异步语义与 staging ring 设计。

## Risks / Trade-offs

- **[`WaitUploadComplete` 阻塞]** 若误在帧内热路径调用会卡住 transfer 队列。→ 缓解：仅析构自动等待 + 测试/消费前显式调用；文档写明语义。
- **[queue 裸指针悬挂]** `pendingQueue` 为裸 `Queue*`。→ 缓解：queue 由 `device` 持有、`device` 生命周期长于资源；析构在资源析构链上，此时 device 必然还在。
- **[staging 段耗尽]** `FrameStagingBuffer::Upload` 在当帧段写满时返回 false（`Allocate` 返回空）。→ 缓解：调用方按 `segmentSize` 预留；v1 不自动扩容。
- **[ring 锁步是契约而非强制]** `numFrames != inflightNum` 或漏调 `Reset()`/`AdvanceFrame()` 会破 in-flight 安全，但编译期无法强制。→ 缓解：spec 写明契约 + 注释 + 测试覆盖；强制校验留待 `aurora-renderer` 接线时加 assert。
- **[当帧 staging 只做 buffer copy]** v1 不支持当帧 staging 纹理上传（`CopyBufferToImage`）。→ 缓解：Non-Goal 明示，后续 change。

## Migration Plan

1. `Buffer.h`：`StaticBuffer` 追踪 handle + 完成 API + 析构等待。
2. `Texture.h`：`Texture` 追踪 handle + 完成 API + 析构等待。
3. `FrameStagingBuffer.h`：新增当帧 staging 上传封装。
4. 测试：完成语义断言 + `FrameStagingUpload` 内联 copy 读回验证 + 在途销毁不再报校验错误。
5. archive。

## Open Questions

- 当帧直写纹理（CPU 可写 / render target 每帧更新）何时落地——后续 change。
- `aurora-renderer` 落地时如何强制 `numFrames == inflightNum` 与锁步（assert 位置）。
- 是否需要 `StagingBufferAllocator + CopyBuffer` 的当帧 inline 路径暴露给 resource 层（GPU_ONLY 目标 + 每帧上传）——后续 change。
