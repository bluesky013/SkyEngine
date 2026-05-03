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
- [ ] 2.5 新增 `VulkanSwapChain.h/.cpp`：`vkCreateSurface`（按 platform：Win32/Wayland/Xlib/Cocoa/Android）+ `vkCreateSwapchainKHR` + `vkGetSwapchainImagesKHR` + 包装为 `VulkanImage` + `vkAcquireNextImageKHR` + `vkQueuePresentKHR` + `Resize` 重建
- [ ] 2.6 处理 `VulkanImage` 把外部 swapchain image 当成 owned vs borrowed 的区分（不要 vkDestroyImage swapchain image）
- [ ] 2.7 让 `VulkanInstance` 暴露 surface 创建所需的 instance extensions（`VK_KHR_surface` + 平台 surface ext）
- [x] 2.8 `VulkanQueue::Submit` 中将 `SemaphoreSubmitInfo::stageMask` 转为 `VkPipelineStageFlags2`（复用 `FromPipelineStageFlags` cast；sync1/sync2 基础位值兼容）
- [x] 2.9 修复 `VulkanContext::pools` 按 QueueType 创建 3 个 pool（GRAPHICS / COMPUTE / TRANSFER）

## 3. DX12 后端

- [ ] 3.1 新增 `D3D12Queue.h/.cpp`：包装 `ID3D12CommandQueue`；`Submit` 用 `ExecuteCommandLists` + 对每个 wait/signal semaphore 发 `Wait`/`Signal`；`WaitIdle` 用内部 fence
- [ ] 3.2 修改 `D3D12Device`：把现有 `graphicsQueue/computeQueue/transferQueue` 改为 `D3D12Queue` 管理
- [ ] 3.3 重写 `D3D12Semaphore`：内部持 `ID3D12Fence`；binary 用 auto-incrementing internal counter；timeline 直接用 caller value；timeline 的 host Signal 用 `ID3D12Fence::Signal`
- [ ] 3.4 扩展 `D3D12Fence`：`IsSignaled` 比较 `GetCompletedValue()`；`WaitFor` 用 `SetEventOnCompletion` + `WaitForSingleObjectEx`
- [ ] 3.5 新增 `D3D12SwapChain.h/.cpp`：`IDXGISwapChain3` + `GetBuffer` + 包装为 `D3D12Image`；`Present` 走 `IDXGISwapChain3::Present`；`Resize` 走 `ResizeBuffers`
- [ ] 3.6 DX12 binary semaphore wait 不能阻塞 GPU 上某个 stage（DX12 fence 是 queue 级别）；在 `Submit` 中按队列发 `Wait` 即可，文档化 `stageMask` 在 DX12 上被忽略

## 4. Metal 后端

- [ ] 4.1 新增 `MetalQueue.h/.mm`：包装 `id<MTLCommandQueue>`；`Submit` 把 `MetalCommandBuffer` commit；wait/signal semaphore 用 `encodeWaitForEvent:value:` / `encodeSignalEvent:value:`；`WaitIdle` 用一个内部 MTLSharedEvent
- [ ] 4.2 修改 `MetalDevice`：从单一 `commandQueue` 扩展成 3 个 `MetalQueue`（GLES 风格：3 类 type 可共享）
- [ ] 4.3 重写 `MetalSemaphore`：BINARY 用 `id<MTLEvent>`，TIMELINE 用 `id<MTLSharedEvent>`；host Signal 用 `MTLSharedEvent.signaledValue` setter
- [ ] 4.4 扩展 `MetalFence`：用 `MTLSharedEvent` + `notifyListener:atValue:` 触发 `dispatch_semaphore_t`，`IsSignaled` 查询 `signaledValue`
- [ ] 4.5 实现 `MetalSwapChain`：`CAMetalLayer` 关联 `nextDrawable`；`AcquireNextImage` 包装 drawable 为 `MetalImage`；`Present` 调 `[commandBuffer presentDrawable:]`（在最近一次 Submit 的 cmdbuf 上挂 present）
- [ ] 4.6 注意 Metal 的 Present 需要在 Submit 的 commandBuffer 上挂；本 change 用"延后 commit"策略：Acquire 后下一次该 swapchain 关联的 Submit 自动挂 presentDrawable

## 5. GLES 后端

- [ ] 5.1 新增 `GLESQueue.h/.cpp`：单一逻辑 queue；`Submit` 立即按序回放 `GLESCommandBuffer` 中录制的命令 lambda；`WaitIdle` 用 `glFinish`
- [ ] 5.2 修改 `GLESDevice::GetQueue` 三种 type 都返回同一 `GLESQueue*`
- [ ] 5.3 重写 `GLESSemaphore`：BINARY 用 `EGLSyncKHR` 或 `std::atomic<bool>` + condvar；TIMELINE 用 `std::atomic<uint64_t>` + condvar（host 串行）
- [ ] 5.4 扩展 `GLESFence`：`IsSignaled` / `WaitFor` 用 `glClientWaitSync`（GL_SYNC_FLUSH_COMMANDS_BIT + 0 timeout 查询）
- [ ] 5.5 实现 `GLESSwapChain`：用 EGL：`eglCreateWindowSurface` + `eglSwapBuffers`；`AcquireNextImage` 直接返回 0（单 backbuffer 模型）；`GetImage(0)` 返回包装 default framebuffer 的 `GLESImage`；`Resize` 调 `eglSwapInterval` / 重建 surface

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
