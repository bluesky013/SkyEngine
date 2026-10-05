## MODIFIED Requirements

### Requirement: AcquireNextImage 取得待渲染索引

`SwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) -> uint32_t` SHALL 返回下一个可被渲染的 image 索引。

- `signalSema`（如非空）MUST 为 binary semaphore；当 image 真正可用时被 signal
- `fence`（如非空）：image 可用时 fence 被 signal
- `timeoutNs == UINT64_MAX` 表示无限等待
- 返回 `INVALID_INDEX` 表示超时或 swapchain 失效
- 当 surface 或 device 丢失时，后端 MUST 将 swapchain 状态置为 `LOST` 并返回 `INVALID_INDEX`，不得继续无限阻塞

调用方 MUST 在使用 `GetImage(index)` 录制命令前先 wait `signalSema`（在 Queue::Submit 的 waitSemaphores 中传入）或 wait `fence`。

#### Scenario: Acquire 返回有效索引

- **WHEN** swapchain 刚创建并调用 `AcquireNextImage(sema, nullptr, UINT64_MAX)`
- **THEN** 返回的索引在 `[0, GetImageCount())` 范围；后续提交一个 wait `sema` 的 cmdbuf 不会死锁

#### Scenario: Acquire 不传 sema 与 fence 退化为同步

- **WHEN** 调用 `AcquireNextImage(nullptr, nullptr, UINT64_MAX)`
- **THEN** 返回有效索引；后端 MUST 在返回前确保 image 已可用（实现可用 hidden internal sync）

#### Scenario: Surface lost 上报为 LOST

- **WHEN** 关联窗口被销毁导致 surface 丢失后调用 `AcquireNextImage`
- **THEN** 后端 SHALL 返回 `INVALID_INDEX` 且 swapchain 状态 SHALL 为 `LOST`（由调用方/事件层避免再次 acquire）
