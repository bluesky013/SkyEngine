## Context

Aurora 已具备：`SwapChain`（`AcquireNextImage`/`Present`/`Resize`/`GetImage`/`GetFormat`/`GetExtent`）、`Queue::Submit` + `Semaphore`（BINARY/TIMELINE）+ `Fence`、RDG 的 `Import` 与 `AddPresentPass`。缺「可渲染表面」层与多视口下的 frame 编排。

关键约束：
- 后端只 Vulkan / DX12 / Metal（GLES 已放弃）；`namespace sky::aurora`；无新增第三方依赖。
- SwapChain image 由后端 `unique_ptr` 持有（borrowed），RDG 的 `Import` 却要 `ImagePtr`（refcount）→ 不能走 Import 路线（会 double-free），必须走「RDG 直接绑定 viewport、prepare 阶段解析裸 `Image*`」。
- 多 viewport（主窗 + 编辑器小窗 + thumbnail）需共享全局 inflight frame，否则共享 dynamic buffer 不稳定。

## Goals / Non-Goals

**Goals:**

- 补全 `RenderViewport` 基类契约（`Begin`/`Acquire`/`Release` + 表面访问器）。
- 落地 `ClientViewport`（swapchain-backed），持有 per-image acquire/render-done sema。
- `SwapChain` 增加状态自查 + 重建（`SwapChainStatus` / `GetStatus`）。
- RDG `BindViewport`（`ViewportImageTag`）+ prepare 阶段 `Acquire`。
- inflight frame 全局化（`DeviceFrameContext` 持 frame index + N fence）。
- 补齐 DX12 `D3D12SwapChain` present。
- 修正 `RenderDeviceExclusive::BeginViewport/EndViewport`。

**Non-Goals:**

- 帧主循环 / renderer（`aurora-renderer`）；编辑器小窗/thumbnail 具体类（未来继承基类）；GLES；swapchain 重建触发时机（触发由窗口尺寸变化驱动，本 change 只做重建动作）。

## Decisions

### 1. `RenderViewport` 基类 = 表面契约，三阶段生命周期

```cpp
class RenderViewport {
public:
    virtual ~RenderViewport() = default;

    virtual bool Begin() = 0;    // check swapchain 状态 + 重建；false = 取消当帧 present
    virtual bool Acquire() = 0;  // 拿下一帧 backbuffer（RDG prepare 阶段调用）
    virtual void Release() = 0;  // present；取消帧为 no-op

    virtual Image       *GetBackbuffer() const = 0;  // Acquire 后有效
    virtual PixelFormat  GetFormat() const = 0;
    virtual Extent2D     GetExtent() const = 0;
    virtual const Name  &GetName() const = 0;

    virtual Semaphore *GetAcquireSemaphore() const = 0;    // 当前帧 ring 的 binary sema
    virtual Semaphore *GetRenderDoneSemaphore() const = 0; // 当前帧 ring 的 binary sema
};
```

基类不含 swapchain 概念、不含 frame index、不含 fence —— 这些留给 `ClientViewport` / `DeviceFrameContext`。`Begin`/`Acquire` 分离的动机：`Begin`（check+重建）必须早于 RDG setup（保证 extent/format 稳定），`Acquire` 留在 prepare（拿 image + signal acquire sema）。

**备选**：`Acquire()` 直接返回 `Image*` 而非 bool → 未采用，Acquire 失败（out-of-date/timeout）与拿到指针是两回事。

### 2. `ClientViewport` = swapchain-backed 具体实现

```cpp
class ClientViewport : public RenderViewport {
public:
    bool Init(Device *device, const SwapChain::Descriptor &desc);

    bool Begin() override;    // GetStatus(): OUT_OF_DATE → Resize; LOST/Resize 失败 → mFrameValid=false, return false
    bool Acquire() override;  // AcquireNextImage(acquireSema[slot], nullptr, UINT64_MAX); INVALID → false
    void Release() override;  // if (mFrameValid) Present(imageIndex, 1, &renderDoneSema[slot])

private:
    SwapChainPtr mSwapChain;
    std::vector<SemaphorePtr> mAcquireSemas;     // imageCount 个（按 per-viewport 帧 ring 索引）
    std::vector<SemaphorePtr> mRenderDoneSemas;  // imageCount 个
    uint32_t mFrameSlot   = 0;   // per-viewport acquire 计数器（ring index）
    uint32_t mCurrentSlot = 0;
    uint32_t mImageIndex  = INVALID_INDEX;
    bool     mFrameValid  = false;
};
```

- sema 按 **per-viewport 帧 ring 索引**（本地 `mFrameSlot` 计数器，不是 image index、不是全局 frame index），与全局 frame 解耦。
- **无 frame index、无 fence**（全局化，见决策 5）。
- `mFrameValid`：`Begin()` 置 true，`Acquire()` 失败置 false；`Release()` 只在 `mFrameValid` 时 present。

### 3. `SwapChain` 状态自查 + 重建

```cpp
enum class SwapChainStatus : uint32_t { OK, OUT_OF_DATE, LOST };
virtual SwapChainStatus GetStatus() const = 0;
```

- **状态自查**：surface 尺寸由**原生窗口**决定，RHI 层不决定目标尺寸；`GetStatus()` 由后端查询 native window（`Descriptor.window`）当前尺寸，与自身 extent 比对：
  - 一致 → `OK`；
  - 不一致 → `OUT_OF_DATE`；
  - surface 丢失 → `LOST`。
- Vulkan 额外反映 `vkAcquireNextImageKHR` 的 `OUT_OF_DATE` / surface lost；`SUBOPTIMAL` **当 `OK` 处理**（本帧照渲，下帧重建）。
- `Begin()`：`OUT_OF_DATE` → `Resize(...)` 重建；`LOST` / 重建失败 → 取消当帧 present。

### 4. RDG `BindViewport` + prepare 阶段 Acquire

```cpp
RDGTextureHandle RenderGraph::BindViewport(const Name &name, RenderViewport *viewport);
```

- 新增 `ResourceTag` 变体 `ViewportImageTag`（存 `RenderViewport*`），`ResourceNode::tag` 扩展。
- 该资源是 **culling 种子**（同 Import）：其 lastWriterPass 恒 live。
- **prepare 阶段**（`BindTransientResources`，`Compile()` 第 5 步）：
  ```cpp
  if (viewport->Acquire()) mResolvedImages[i] = viewport->GetBackbuffer();  // 裸指针，无 refcount
  else                     /* unresolved → 写它的 pass 被 cull；Release 变 no-op */
  ```
- 裸 `Image*` 直接进 `mResolvedImages`（本就 `TransientVector<Image*>` 不 refcount），image 生命周期由 viewport 的 Acquire/Release 窗口保证 → 无 `CounterPtr` 包装、无 double-free。
- Present 语义仍在 RDG 外：`AddPresentPass` 只做 PRESENT barrier/culling 标记，`Present` 由 `Release()` 执行。

### 5. inflight frame 全局化（`DeviceFrameContext`）

多 viewport 共享**同一个全局 frame index + 同一组 fence**，保证按 `frameIndex % N` 分配的共享 dynamic buffer（`BatchAllocator`/`GlobalRenderResources`/`TransientBufferPool`）同帧稳定。

```cpp
class DeviceFrameContext {
    uint32_t mFrameIndex  = 0;
    uint32_t mInflightNum = 2;
    std::vector<FencePtr> mFences;   // N 个全局 in-flight fence
    FrameAllocator mFrameAllocator;  // per-frame arena
    // ... 共享 dynamic buffer，按 mFrameIndex % N 分配 ...

    void   BeginFrame();      // Wait fence[F % N] + Reset + ++F（保证 dynamic buffer 可复用）
    void   EndFrame();        // Reset frame allocator
    Fence *GetFrameFence();   // fence[F % N] —— 供 SubmitInfo.fence
};
```

- fence 从 viewport 移除，改由 `DeviceFrameContext` 持有；wait/signal 都走 frameContext。
- 多 viewport 一帧内 `SubmitInfo.fence = 全局 fence[F % N]`（fence 由 Driver 在多个 submit 间复用，最后一个完成的信号 → 帧完成）。

### 6. 各后端 present 映射

| 后端 | Acquire | render-done | Present |
|---|---|---|---|
| Vulkan | `vkAcquireNextImageKHR` signal binary | submit signal binary | `vkQueuePresentKHR` wait render-done sema |
| DX12 | `GetCurrentBackBufferIndex`（acquire sema 立即 signal） | `ID3D12Fence::Signal` | `IDXGISwapChain3::Present` |
| Metal | `nextDrawable` + 立即 signal | `MTLEvent` signal | `presentDrawable:` 前 `encodeWaitForEvent` |

`ClientViewport` 后端无关；差异封闭在 `SwapChain` 实现。DX12 的 `D3D12SwapChain`（`IDXGISwapChain3` + `GetBuffer` + `Present` + `Resize` + `GetStatus`）作为本 change 任务补齐。

### 7. `RenderDeviceExclusive` 适配

修正 `BeginViewport` 的断言（`SKY_ASSERT(mCurrentViewport != nullptr)` → `== nullptr`），`BeginViewport/EndViewport` 记录/清空当前 surface。

## 帧生命周期时序

```
RenderDeviceExclusive / frame driver
 1. frameContext->BeginFrame()          // Wait 全局 fence[F%N] → ++F
 2. 对每个 viewport：
      if (!viewport->Begin()) continue  // check 状态 → 重建 → 失败取消 present（skip 整帧）
 3. 建 RDG：graph->BindViewport(name, viewport) + 各 pass 写 backbuffer + AddPresentPass
 4. graph->Compile()                    // prepare：viewport->Acquire() → resolve backbuffer
 5. graph->Execute(cmdBuf)
 6. queue->Submit({cmdBuf, wait=acquireSema, signal=renderDoneSema, fence=全局 fence[F%N]})
 7. viewport->Release()                 // Present(imageIndex, 1, &renderDoneSema)；取消帧 no-op
 8. frameContext->EndFrame()            // 帧末 signal 全局 fence
```

## Risks / Trade-offs

- **[Acquire 失败 mid-frame]** prepare 阶段 Acquire 失败时 RDG 已建好 → 缓解：backbuffer 资源 unresolved → 写它的 pass 被 cull，`Release` 变 no-op。
- **[Begin 阻塞]** `Begin` 里的重建（`Resize`）可能触发 `WaitIdle` → 缓解：重建只在 `OUT_OF_DATE` 时发生，触发频率低；`SUBOPTIMAL` 当 OK 不重建。
- **[全局 fence 与多 submit]** 多个 submit 复用同一 fence，最后一个完成者 signal → 缓解：fence 只做「帧级 in-flight」粒度，不要求 per-viewport 完成时序。
- **[裸指针生命周期]** `mResolvedImages[i]` 存 `GetBackbuffer()` 裸指针 → 缓解：image 由 viewport 在 Acquire/Release 窗口持有，graph 生命周期在窗口内；不跨帧持有。

## Migration Plan

1. `SwapChain` 加 `SwapChainStatus`/`GetStatus`（三后端）。
2. `RenderViewport` 基类扩展（三阶段 + 访问器）。
3. `ClientViewport` 实现（Begin/Acquire/Release + per-image sema）。
4. `DeviceFrameContext` 加全局 fence + BeginFrame/EndFrame。
5. RDG `BindViewport` + `ViewportImageTag` + prepare acquire + culling 种子。
6. 补齐 DX12 `D3D12SwapChain` present。
7. 修正 `RenderDeviceExclusive`。
8. 测试 + archive。
