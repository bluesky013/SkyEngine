## ADDED Requirements

### Requirement: SwapChain 创建与基础属性查询

`Device::CreateSwapChain(const SwapChain::Descriptor &)` SHALL 返回一个完整的 `SwapChain` 实例。`Descriptor` MUST 包含 `void* window`、`uint32_t width`、`uint32_t height`、`PixelFormat preferredFormat`、`PresentMode preferredMode`。

后端 MAY 协商出与 `preferredFormat` / `preferredMode` 不同的实际 format / mode；调用方 MUST 通过 `GetFormat()` 取得最终使用的 format。

`SwapChain` SHALL 至少提供以下查询：
- `GetImageCount() -> uint32_t`：swapchain 中可呈现 image 的数量
- `GetFormat() -> PixelFormat`：实际使用的 color format
- `GetExtent() -> Extent2D`：当前 image 尺寸
- `GetImage(uint32_t index) -> Image*`：取得索引对应的 backbuffer

`GetImageCount()` MUST 返回 ≥ 2。`GetImage(i)` 对所有 `0 ≤ i < GetImageCount()` MUST 返回非空 `Image*`，该 `Image` 持有 `ImageUsageFlagBit::RENDER_TARGET`。

#### Scenario: 创建并查询 swapchain
- **WHEN** 调用方在持有 native window 的环境下创建 `SwapChain(800, 600, BGRA8_UNORM, IMMEDIATE)`
- **THEN** 返回非空 `SwapChain*`；`GetImageCount() ≥ 2`；`GetExtent() == {800, 600}`；`GetFormat()` 是后端实际选用的 format

#### Scenario: SwapChain image 可作为 ColorAttachment
- **WHEN** 调用方对 `swapchain->GetImage(0)` 构造 `RenderingInfo::colors[0].image` 并 BeginRendering
- **THEN** Encoder 接受该 image，能正常 BeginRendering / EndRendering

### Requirement: AcquireNextImage 取得待渲染索引

`SwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) -> uint32_t` SHALL 返回下一个可被渲染的 image 索引。

- `signalSema`（如非空）MUST 为 binary semaphore；当 image 真正可用时被 signal
- `fence`（如非空）：image 可用时 fence 被 signal
- `timeoutNs == UINT64_MAX` 表示无限等待
- 返回 `INVALID_INDEX` 表示超时或 swapchain 失效

调用方 MUST 在使用 `GetImage(index)` 录制命令前先 wait `signalSema`（在 Queue::Submit 的 waitSemaphores 中传入）或 wait `fence`。

#### Scenario: Acquire 返回有效索引
- **WHEN** swapchain 刚创建并调用 `AcquireNextImage(sema, nullptr, UINT64_MAX)`
- **THEN** 返回的索引在 `[0, GetImageCount())` 范围；后续提交一个 wait `sema` 的 cmdbuf 不会死锁

#### Scenario: Acquire 不传 sema 与 fence 退化为同步
- **WHEN** 调用 `AcquireNextImage(nullptr, nullptr, UINT64_MAX)`
- **THEN** 返回有效索引；后端 MUST 在返回前确保 image 已可用（实现可用 hidden internal sync）

### Requirement: Present 呈现图像到屏幕

`SwapChain::Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas)` SHALL 将 `imageIndex` 对应的 image 呈现到关联的 native window。

Present MUST 等待 `waitSemas` 中所有 binary semaphore signal 后才将 image 提交给 windowing system。

调用方 MUST 在 Present 之前 Submit 含该 image 渲染命令的 cmdbuf，并把 cmdbuf signal 的 binary semaphore 作为 `waitSemas` 之一传入。

#### Scenario: 完整一帧 clear-screen
- **WHEN** 依次调用：Acquire(signalSemaA)；录制 cmdbuf 把 image clear 为已知颜色；Submit（waitSemaA + signalSemaB）；Present(imageIndex, 1, &semaB)
- **THEN** 整个流程不报错；`device->WaitIdle()` 之后下一帧 Acquire 又返回有效索引（说明 swapchain 仍处于工作状态）

### Requirement: Resize 在窗口尺寸变化时重建

`SwapChain::Resize(uint32_t width, uint32_t height)` SHALL 释放当前 swapchain 的全部 image 并按新尺寸重新创建。Resize 之前持有的 `Image*`（来自 `GetImage`）在 Resize 后**全部失效**，调用方 MUST 在 Resize 之后重新调用 `GetImage` 取得新指针。

Resize 时如有未完成的 Submit 涉及旧 image，调用方 MUST 先 `device->WaitIdle()`（或等价的 fence 等待），否则行为未定义。

#### Scenario: Resize 后 GetExtent 反映新尺寸
- **WHEN** 在 swapchain（800×600）上调用 `device->WaitIdle()` 后 `Resize(1024, 768)`
- **THEN** `GetExtent() == {1024, 768}`；`GetImage(0)` 返回新指针（与 Resize 前的指针不要求相等）；`GetImageCount()` MAY 改变也 MAY 不变

#### Scenario: Resize 至零尺寸
- **WHEN** 调用 `Resize(0, 0)`（窗口最小化）
- **THEN** Resize 不崩溃；后续 `AcquireNextImage` MAY 返回 `INVALID_INDEX`；调用方应在窗口恢复时再次 Resize 到非零尺寸

### Requirement: SwapChain 销毁

`SwapChain` 销毁时 SHALL 释放所有 backbuffer image、native swapchain handle、与之关联的 surface。销毁前调用方 SHOULD 确保该 swapchain 的所有 in-flight Present 已完成（一般通过 `device->WaitIdle()`）。

#### Scenario: 正常销毁
- **WHEN** 调用 `device->WaitIdle()` 后释放最后一个 `CounterPtr<SwapChain>`
- **THEN** 析构正常完成，不泄漏 native handle（valgrind / VkValidationLayer / D3D12 debug layer 不报错）
