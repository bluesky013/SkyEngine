## Why

Aurora RHI 当前只能创建 Device、分配 CommandBuffer 并通过 Encoder 录制命令，但**无法把命令送到 GPU 执行，也无法把结果呈现到屏幕**。所有 encoder 测试都以 `device->WaitIdle()` 结尾，说明根本没有 submit 路径。SwapChain 类除了 `Descriptor` 之外是空的，没有 Acquire / Present / Resize。要让 aurora 推进到 "渲染并 present 一帧 clear-screen" 这条最小可用路径，必须先把 Queue / Submit / Present 三件事补齐。这一层是 Barrier、ResourceGroup、RDG 后续工作的前置条件。

## What Changes

- 在接口层引入 **`Queue`** 抽象，对外暴露 graphics / compute / transfer 三种队列类型；底层不一定是不同的硬件队列（Vulkan 上可能复用 graphics queue family）
- **`Device::GetQueue(QueueType)`** 取代当前隐藏的 `graphicsQueue/computeQueue/transferQueue` 成员
- 新增 **`Queue::Submit(const SubmitInfo&)`** 接受一组 `CommandBuffer*` + wait/signal semaphores + 可选 fence
- 扩展 **`SwapChain`**：`AcquireNextImage(timeout, signalSema, fence) -> uint32_t`、`Present(waitSema)`、`Resize(w, h)`、`GetImage(index)`、`GetFormat()`、`GetImageCount()`
- **`Semaphore`** 区分 binary 与 timeline 两种类型；`SubmitInfo` 接受带 value 的 timeline wait/signal
- **`Fence`** 增加 `IsSignaled()` 用于轮询（已有 `Wait/Reset`）
- 在 4 个后端落地：Vulkan（已有 timelineSemaphore 假设）、DX12（ID3D12Fence 模拟二者）、Metal（MTLEvent / drawable）、GLES（软件 fence + EGL/AGL swap）
- 新增 **集成测试**：clear-screen → present 端到端跑通；多线程录制→单队列 submit；timeline semaphore wait/signal 链
- **BREAKING**：现有 `Fence::Wait/Reset` 语义保持，但 Semaphore::Descriptor 增加 `type` 字段（默认 BINARY 兼容现有调用）

## Capabilities

### New Capabilities
- `aurora-queue`: 队列抽象与 submit 语义（按 QueueType 路由、wait/signal semaphore 处理、fence 完成通知）
- `aurora-swapchain`: SwapChain 完整生命周期（创建、Acquire、Present、Resize、format 协商、image 数量查询）
- `aurora-sync-primitives`: Fence + Semaphore 在 Submit / Acquire / Present 中的统一语义（含 timeline semaphore）

### Modified Capabilities
（无既有 spec 修改：openspec/specs/ 当前只有 console / cvar / shell 等无关项；aurora 系列均为新建）

## Impact

- **接口头文件**：`aurora/rhi/Device.h`、`aurora/rhi/SwapChain.h`、`aurora/rhi/Semaphore.h`、`aurora/rhi/Fence.h` 新增方法/类型；新增 `aurora/rhi/Queue.h`、`aurora/rhi/SubmitInfo.h`
- **后端实现**：4 套 `*Device` / `*SwapChain` / `*Semaphore` / `*Fence` / 新增 `*Queue` 文件
- **测试**：`engine/aurora/rhi/test/SyncTest.cpp` 扩展；新增 `SubmitTest.cpp` / `SwapChainTest.cpp`
- **调用方**：暂无（aurora 还未被任何 module 使用）；`engine/aurora/core/Renderer` 后续一帧主循环将基于此构建
- **平台依赖**：Metal 需要 `CAMetalLayer`、Windows 需要 HWND 接入 SwapChain；测试中通过 headless（Vulkan offscreen + 不创建 SwapChain）路径验证 Submit 不依赖 SwapChain
