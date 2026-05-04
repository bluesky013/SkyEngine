## 1. 接口层（Aurora.RHI 头文件）

- [x] 1.1 新增 `aurora/rhi/Queue.h`：`Queue` 抽象类（`Submit` / `WaitIdle` / `GetType`）
- [x] 1.2 新增 `aurora/rhi/SubmitInfo.h`：`SemaphoreSubmitInfo`、`SubmitInfo` 结构体
- [x] 1.3 修改 `aurora/rhi/Semaphore.h`：加 `SemaphoreType` 枚举、`Descriptor::type`、`GetType()` / `Signal(value)` / `Wait(value, timeoutNs)` / `GetCurrentValue()` 接口
- [x] 1.4 修改 `aurora/rhi/Fence.h`：加 `IsSignaled()` / `WaitFor(timeoutNs)` 接口
- [x] 1.5 修改 `aurora/rhi/SwapChain.h`：加 `AcquireNextImage` / `Present` / `Resize` / `GetImage` / `GetImageCount` / `GetFormat` / `GetExtent` 接口
- [x] 1.6 修改 `aurora/rhi/Device.h`：加 `GetQueue(QueueType)`（`CreateResourceGroup` rename 已在 aurora-quick-fixes 中完成）
- [x] 1.7 把 `Queue.h` / `SubmitInfo.h` 加到 `Device.h` 的 include 顶部聚合处

## 2. Vulkan 后端

- [x] 2.1 新增 `VulkanQueue.h/.cpp`：实现 `Submit` 用 `vkQueueSubmit2`、`WaitIdle` 用 `vkQueueWaitIdle`；持有 VkQueue + queueFamilyIndex
- [x] 2.2 修改 `VulkanDevice`：取消内部成员暴露，改为 `std::array<std::unique_ptr<VulkanQueue>, 3>`；`GetQueue` 直接索引返回；启用 sync2 feature；旧 `Device::WaitIdle` 仍走 `vkDeviceWaitIdle`
- [x] 2.3 重写 `VulkanSemaphore`：按 `Descriptor::type` 创建 binary 或 timeline；timeline 走 `vkSignalSemaphore` / `vkWaitSemaphores` / `vkGetSemaphoreCounterValue`
- [x] 2.4 扩展 `VulkanFence`：`IsSignaled` 用 `vkGetFenceStatus`；`WaitFor` 用带 timeout 的 `vkWaitForFences`
- [x] 2.5 新增 `VulkanSwapChain.h/.cpp`：metal/win32/android surface 创建链路；`vkCreateSwapchainKHR` + format/presentMode/extent 协商；`vkGetSwapchainImagesKHR` 包装为 borrowed `VulkanImage`；`vkAcquireNextImageKHR`、`vkQueuePresentKHR`、`Resize` 重建
- [x] 2.6 `VulkanImage::InitFromSwapChain` 加 PixelFormat 参数；`owned=false` 已就位（析构不调 vmaDestroyImage / vkDestroyImage）
- [x] 2.7 instance ext `VK_KHR_surface` + 平台 surface ext 已在 VulkanInstance 中启用；本轮加 `VulkanFunctions` 表面/swapchain 函数指针 + CMake `VK_USE_PLATFORM_*` 宏
- [x] 2.8 `VulkanQueue::Submit` 中将 `SemaphoreSubmitInfo::stageMask` 转为 `VkPipelineStageFlags2`（复用 `FromPipelineStageFlags` cast；sync1/sync2 基础位值兼容）
- [x] 2.9 修复 `VulkanContext::pools` 按 QueueType 创建 3 个 pool（GRAPHICS / COMPUTE / TRANSFER）

## 3. DX12 后端

- [x] 3.1 新增 `D3D12Queue.h/.cpp`：包装 `ID3D12CommandQueue`；`Submit` 用 `ExecuteCommandLists` + queue->Wait/Signal；`WaitIdle` 用内部 fence + event（Windows 待回归）
- [x] 3.2 修改 `D3D12Device`：3 个 `std::unique_ptr<D3D12Queue>` 替换原始 ComPtr 队列；`GetQueue` 索引返回；`WaitIdle` 委托队列；移除老 `fence` 成员
- [x] 3.3 扩展 `D3D12Semaphore`：本 change 在 quick-fixes 阶段已加 GetType/Signal/Wait/GetCurrentValue；本轮加 `AdvanceBinarySignalValue/GetBinaryWaitValue` 给 D3D12Queue 用
- [x] 3.4 扩展 `D3D12Fence`：本 change 在 quick-fixes 阶段已加 IsSignaled/WaitFor；本轮加 `BumpPendingValue` 给 D3D12Queue::Submit 用
- [ ] 3.5 新增 `D3D12SwapChain.h/.cpp`：`IDXGISwapChain3` + `GetBuffer` + 包装为 `D3D12Image`；`Present` 走 `IDXGISwapChain3::Present`；`Resize` 走 `ResizeBuffers`
- [ ] 3.6 DX12 binary semaphore wait 不能阻塞 GPU 上某个 stage（DX12 fence 是 queue 级别）；在 `Submit` 中按队列发 `Wait` 即可，文档化 `stageMask` 在 DX12 上被忽略

## 4. Metal 后端

- [x] 4.1 新增 `MetalQueue.h/.mm`：包装 `id<MTLCommandQueue>`；`Submit` 把 `MetalCommandBuffer` commit；wait/signal sema 用 `encodeWaitForEvent:value:` / `encodeSignalEvent:value:`；fence 用 encodeSignalEvent + notifyListener；`WaitIdle` 用空 cmdbuf + waitUntilCompleted
- [x] 4.2 修改 `MetalDevice`：3 个独立 `MetalQueue`（每个持自己的 `id<MTLCommandQueue>`）；`GetQueue` 索引返回；`CreateCommandPool` 把对应 queue handle 传给 pool；`GetCommandQueue` 保留为 graphics queue 别名
- [x] 4.3 重写 `MetalSemaphore`：统一用 `id<MTLSharedEvent>`；BINARY 内部维护 `binaryValue` counter（每次 signal/wait 隐式 +1）；TIMELINE 用 caller value；host `Signal/Wait` 经 `signaledValue` / `waitUntilSignaledValue:timeoutMS:`
- [x] 4.4 扩展 `MetalFence`：`id<MTLSharedEvent>` + `notifyListener:atValue:block:` 在 GPU 完成时回调；CPU 端 mutex+condvar 维持 `Wait/IsSignaled/WaitFor` 三件套
- [x] 4.5 实现 `MetalSwapChain`：`CAMetalLayer` + `nextDrawable`；MetalImage 加 `RebindBorrowed/Reset` 包装外部 drawable.texture；Acquire 拿 drawable + 立即 signal sema/fence（Metal 无原生 acquire 信号）；Present 用独立 cmdbuf encodeWaitForEvent + presentDrawable + commit；Resize 改 drawableSize
- [x] 4.6 Present 策略：每次 Present 起一个 fresh `[graphicsQueue commandBuffer]` 挂 wait+presentDrawable，避免与用户 Submit 链耦合（比设计文档的"延后 commit"路径简单且解耦更清晰）

## 5. GLES 后端

- [x] 5.1 新增 `GLESQueue.h/.cpp`：单一逻辑 queue；`Submit` 做 wait→no-op→signal→fence 链（GLES 命令录制即执行，Submit 主要做 CPU sync 语义）；`WaitIdle` 用 `glFinish`
- [x] 5.2 `GLESDevice::GetQueue` 三种 type 全部返回同一个 `GLESQueue*`
- [x] 5.3 改 `GLESSemaphore`：用 mutex+condvar 让 Wait 阻塞，Signal notify_all；保持 binary/timeline 区分（binary 由 Submit 路径隐式 +1）
- [x] 5.4 `GLESFence` 加 `SignalFromQueue`：在 Submit 完成时插 `glFenceSync`；`IsSignaled` / `WaitFor` 已就位
- [x] 5.5 实现 `GLESSwapChain`：`AcquireNextImage` 立即 signal sema/fence + 返回 0；`Present` block 在 wait sema 后调 `eglSwapBuffers`；`GetImage(0)` 暂返 nullptr（默认 FBO 0 的 Image 包装属于后续 RG/RDG 范畴）；Resize 已就位

## 6. 测试

- [x] 6.1 扩展 `AuroraTestHelper`：加 `MakeBinarySema(device)` / `MakeTimelineSema(device, initial)` / `MakeFence(device)` 工具
- [x] 6.2 新增 `SubmitTest.cpp`：8 个 Vulkan headless 测试全绿
  - [x] 6.2.1 `SubmitEmptyCmdBufWithFence`：空 cmdbuf + fence；fence 完成查询正确
  - [x] 6.2.2 `BinarySemaphoreChainBetweenSubmits`：A signal sema → B wait sema
  - [x] 6.2.3 `TimelineSemaphoreCrossSubmit`：跨两次 Submit 用同一 timeline value
  - [x] 6.2.4 `MultiThreadRecordSingleSubmit`：4 个线程各录一段 cmdbuf，主线程一次 Submit
  - [x] 6.2.5 `FenceWaitForZeroReturnsFalseBeforeCompletion`：非阻塞 WaitFor(0) 路径
  - [x] 6.2.6 `GetGraphicsQueue` / `QueueWaitIdle` / `TimelineHostSignalAndWait` 补充覆盖
- [ ] 6.3 新增 `SwapChainTest.cpp`：用 SDL 创建隐藏 native window；CI 没 GPU 时跳过
  - [ ] 6.3.1 `CreateAndQueryProperties`：format / extent / imageCount
  - [ ] 6.3.2 `AcquireRenderPresentLoop`：跑 30 帧 clear-screen 不崩溃
  - [ ] 6.3.3 `ResizeAndContinue`：第 10 帧 Resize 到新尺寸继续渲染
- [x] 6.4 扩展 `SyncTest.cpp`：timeline value 单调性、host signal、cross-thread wait（覆盖在 SubmitTest::TimelineHostSignalAndWait + TimelineSemaphoreCrossSubmit 中）
- [x] 6.5 顺手验证 Submit 路径：SubmitTest 端到端覆盖"录制 + Submit + fence wait"链；现有 EncoderTest 仍用 device->WaitIdle 作为占位（这些测试只录空 cmdbuf 不 Submit，无需改造）

## 7. 收尾 / 文档

- [ ] 7.1 在 `engine/aurora/` 加一个简短的 `AGENTS.md`：说明 Queue / SwapChain / Semaphore 的契约、binary vs timeline 用法、4 后端的能力差异表（Metal Present 延后 commit / GLES 单 queue / DX12 stageMask 忽略）
- [ ] 7.2 在 `engine/aurora/rhi/test/` 跑 `AuroraTest --gtest_filter=Submit*:Sync*:SwapChain*` 全绿（至少 Vulkan + 当前 host 平台第二后端）
- [ ] 7.3 跑 `cmake --build` 在 macOS / Windows（如能）/ Linux 上各通过一次
- [ ] 7.4 验证 Vulkan validation layer / D3D12 debug layer / Metal validation 不报新 warning
- [ ] 7.5 archive 本 change：`openspec archive aurora-queue-submit-present`
