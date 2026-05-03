## Context

Aurora 是 SkyEngine 在 `dev_refactor_rhi` 分支上重写的 RHI 层（取代旧 `engine/rhi`）。当前接口层提供 Device / CommandBuffer / Encoder / Buffer / Image / Shader / Pipeline / SwapChain / Fence / Semaphore，但**提交（submit）与呈现（present）路径完全缺失**：

- `Device` 没有 Submit 接口
- `Queue` 类不存在；4 个后端各自在 `*Device` 内部持有 native queue handle 但不对外暴露
- `SwapChain` 仅有 `Descriptor`，没有方法
- `Semaphore` 只是空 RefObject 壳；没有 binary / timeline 区分
- `Fence` 有 `Wait/Reset` 但没有非阻塞查询

后端现状：Vulkan 已强制要求 `dynamicRendering` + `timelineSemaphore`；DX12 已创建 graphics/compute/transfer 三个 `ID3D12CommandQueue`；Metal 持有 `id<MTLCommandQueue>`；GLES 走软件命令列表 + 立即模式 swap。Aurora 还没有任何上层调用方，所以这次改动是纯增量、零迁移负担。

约束：
- 4 个后端都要有最小可工作实现（不能只做 Vulkan）
- 接口必须能在 GLES 这种"无 timeline / 无真正 multi-queue"的后端上合理降级
- 不引入新的第三方依赖

## Goals / Non-Goals

**Goals:**
- 定义 `Queue` 抽象，按 `QueueType`（GRAPHICS / COMPUTE / TRANSFER）路由 submit
- 定义 `SubmitInfo`：commandBuffers + waitSemaphores + signalSemaphores + 可选 fence；wait/signal 项支持 timeline value
- 让 `SwapChain` 跑通 Acquire → 录制 → Submit → Present 一帧 clear-screen
- 4 个后端各自落地，至少 Vulkan 与 Metal 跑通端到端 present；DX12 与 GLES 至少跑通 headless submit + present 接口编译/桩
- 测试覆盖 timeline semaphore wait/signal 链、fence 完成查询、跨线程录制单线程 submit

**Non-Goals:**
- 不实现 multi-GPU / cross-adapter
- 不实现 sparse binding / sparse residency
- 不引入 Barrier API（独立 change，是下一项）
- 不改 ResourceGroup / Descriptor 设计（独立 change）
- 不实现 RDG（依赖于本 change 完成后才动）
- 不优化 swapchain 重建路径（只做 Resize 接口；触发条件交给上层）

## Decisions

### 决策 1：`Queue` 作为独立类，从 `Device::GetQueue(QueueType)` 取得

```cpp
class Queue {
public:
    virtual ~Queue() = default;
    virtual void Submit(const SubmitInfo &info) = 0;
    virtual void WaitIdle() = 0;
    virtual QueueType GetType() const = 0;
};
```

`Device` 持有 `std::array<std::unique_ptr<Queue>, 3>`，按 QueueType 索引返回。

**Why:** 把 submit 从 Device 上下移到 Queue，符合 Vulkan/D3D12/Metal 的物理模型；上层 RDG 后续可以按队列做并行 submit。`Device::Submit(QueueType, ...)` 也是可选 API（备选），但 Queue-first 更可扩展。

**Alternatives considered:**
- *Device::Submit(QueueType, ...)*：更扁平、对调用者更简单，但把跨队列协调（timeline semaphore）的状态藏在 Device 上不利于后续 RDG
- *暴露 native queue handle*：失去抽象，被否决

### 决策 2：`SubmitInfo` 用 timeline-aware 的统一结构

```cpp
struct SemaphoreSubmitInfo {
    Semaphore *semaphore = nullptr;
    uint64_t   value     = 0;            // binary 时忽略
    PipelineStageFlags stageMask = PipelineStageBit::TOP;
};

struct SubmitInfo {
    std::vector<CommandBuffer*>           commandBuffers;
    std::vector<SemaphoreSubmitInfo>      waitSemaphores;
    std::vector<SemaphoreSubmitInfo>      signalSemaphores;
    Fence                                *fence = nullptr;   // 可选
};
```

**Why:** Vulkan timelineSemaphore 已是必备前提，binary 是其退化形态（value = 0）。统一结构避免接口分裂；后端按 Semaphore 实际类型分发。

### 决策 3：`Semaphore::Descriptor` 加 `type` 字段，binary 默认

```cpp
enum class SemaphoreType { BINARY, TIMELINE };

struct Descriptor {
    SemaphoreType type = SemaphoreType::BINARY;
    uint64_t initialValue = 0;   // 仅 timeline 使用
};
```

`Semaphore` 上加 `GetType()`、`uint64_t Signal(uint64_t value)`（仅 timeline，host 端 signal）、`bool Wait(uint64_t value, uint64_t timeoutNs)`（仅 timeline）。

**Why:** SwapChain Acquire 通常用 binary semaphore（VK 要求）；跨队列同步用 timeline。两者必须共存。

### 决策 4：SwapChain 把 image 暴露为 `Image*` 引用而非另一个抽象

```cpp
class SwapChain : public RefObject {
public:
    virtual uint32_t   AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) = 0;
    virtual void       Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) = 0;
    virtual void       Resize(uint32_t width, uint32_t height) = 0;
    virtual Image*     GetImage(uint32_t index) const = 0;
    virtual uint32_t   GetImageCount() const = 0;
    virtual PixelFormat GetFormat() const = 0;
    virtual Extent2D   GetExtent() const = 0;
};
```

SwapChain 持有的 `Image*` 在 SwapChain 析构 / Resize 时失效；调用方不持有所有权。

**Why:** 复用现有 `Image` 抽象作为 RTV/SRV 的目标，让 BeginRendering 直接收 `Image*` 不需要特殊分支。Resize 让 image 失效是符合预期的（VK/D3D12/Metal 都是这样）。

**Alternatives considered:**
- *SwapChainImage 独立抽象*：更类型安全，但和 Image 重复 90% 接口

### 决策 5：DX12 后端用 `ID3D12Fence` 模拟 binary semaphore + timeline semaphore + fence

DX12 没有原生 binary semaphore；但 `ID3D12Fence` 本身就是 timeline。
- Binary semaphore = `ID3D12Fence`，每次 signal value 自增
- Timeline semaphore = `ID3D12Fence`，value 由调用方指定
- Fence = `ID3D12Fence` + 内部 event handle

DX12 SwapChain 用 `IDXGISwapChain3` + `WaitForSingleObjectEx`。

### 决策 6：GLES 后端按 EGL/AGL 软实现降级

GLES 没有 multi-queue / 真正 timeline。降级策略：
- 单一"逻辑队列"，所有 QueueType 都返回同一个 GLESQueue
- Submit = 立即调用录制好的命令列表（GLES 已是软命令列表）+ 在末尾 `glFinish` 模拟 fence 完成
- Binary semaphore 用 `EGLSync` / 进程内事件模拟
- Timeline semaphore 用进程内 `std::atomic<uint64_t>` + condvar；不能跨 GPU 同步，但 GLES 不需要
- SwapChain Present = `eglSwapBuffers`

GLES backend 的所有 Queue / Semaphore 实现在 CPU 端串行化，性能不是目标。

### 决策 7：Metal 用 `MTLEvent` / `MTLSharedEvent`

- Binary = `MTLEvent`（同 device 内）
- Timeline = `MTLSharedEvent`（带 64-bit value，跨队列 OK）
- Fence = `MTLSharedEvent` + completion handler 触发 `dispatch_semaphore_t`

SwapChain 基于 `CAMetalLayer::nextDrawable` + `MTLCommandBuffer::presentDrawable:`。

### 决策 8：测试 headless / windowed 分层

- **SubmitTest**：不依赖 SwapChain；创建 buffer、用 BlitEncoder copy、submit + fence wait + 验证内容
- **SwapChainTest**：仅在能创建 native window 的环境运行；用 SDL 创建隐藏窗口（已是依赖）
- **SyncTest 扩展**：timeline value 单调递增、跨线程 wait
- 4 个后端共享 GoogleTest 参数化（已有 `AuroraVulkanTest` 等基类，新增 `AuroraSubmitTestBase` 封装通用 fixtures）

## Risks / Trade-offs

- **GLES timeline 模拟非 GPU 同步** → 缓解：明确 GLES 后端只承诺单 GPU、单线程 submit 的功能正确性，不承诺与 Vulkan 等价的并行性能
- **DX12 用 ID3D12Fence 双重身份**（同时作为 semaphore + fence） → 缓解：在 D3D12Semaphore / D3D12Fence 内部各持有独立 ID3D12Fence handle；不复用，避免 value 冲突
- **SwapChain Resize 让 image 失效** → 缓解：在 spec 里写明 contract；测试覆盖 Resize 后老 imageIndex 的行为
- **Vulkan 未启用 `synchronization2`** → 缓解：当前用经典 `vkQueueSubmit2` 即可（Vulkan 1.3 已有），只在 wait/signal stage mask 上用新 API；不改变 PipelineStageBit 枚举
- **测试需要 native window 才能跑 SwapChain** → 缓解：SwapChain 部分用 conditional skip（CI 通常 headless）；headless 路径覆盖 Submit
- **Semaphore type 字段是 BREAKING（虽然加默认值）** → 缓解：当前没有调用方使用，零迁移成本；落仓时一次性更新 interface + 4 后端

## Migration Plan

无外部调用方，无需迁移。落仓顺序：
1. 接口头先合（Queue.h / SubmitInfo.h / 修改 Semaphore.h / Fence.h / SwapChain.h）
2. Vulkan 后端跑通端到端（含 SwapChain）
3. Metal 后端跟上
4. DX12 / GLES 跟上（DX12 SwapChain 可能晚一步）
5. 测试套件扩展同步落

每一步都是独立 PR，互不阻塞。

## Open Questions

- **是否需要在 `Queue::Submit` 之外提供 `Queue::SubmitAndPresent` 合并接口？** Metal 有 `presentDrawable:` 是 cmdbuffer 的一部分；强制拆开两次调用在 Metal 上是 2 次 enqueue。倾向：先拆开，Metal 内部把 Present 排到 commit 时机；如果有性能问题再加合并接口。
- **GLES 是否需要支持 compute queue？** GLES 3.1 有 compute shader，但 aurora GLES 后端走单逻辑队列，compute 与 graphics 串行。倾向：返回同一 Queue，文档说明。
- **SwapChain 创建是否需要先有 graphics queue 才能选格式？** Vulkan `vkGetPhysicalDeviceSurfaceFormatsKHR` 需要 surface；Acquire 时需要 queue 兼容性。倾向：在 SwapChain Init 时持有 graphics queue 引用。
