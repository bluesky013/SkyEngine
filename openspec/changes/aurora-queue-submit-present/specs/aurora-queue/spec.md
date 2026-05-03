## ADDED Requirements

### Requirement: Queue 抽象按 QueueType 暴露

Aurora `Device` SHALL 通过 `Device::GetQueue(QueueType type)` 返回对应类型的 `Queue` 指针。`Queue` 实例的所有权归 `Device` 所有，调用方不得 delete；`Queue` 在 `Device` 析构后失效。

`QueueType` MUST 至少支持 `GRAPHICS`、`COMPUTE`、`TRANSFER` 三类。在硬件不支持独立 compute / transfer 队列的后端（GLES、或 Vulkan 上某些 GPU），`Device::GetQueue` MAY 对多个类型返回同一 `Queue*`，但行为 MUST 保持正确。

#### Scenario: 取得 graphics queue
- **WHEN** 调用方在已初始化的 `Device` 上调用 `device->GetQueue(QueueType::GRAPHICS)`
- **THEN** 返回非空 `Queue*`，其 `GetType()` 返回 `QueueType::GRAPHICS`

#### Scenario: 多次取同一队列返回相同实例
- **WHEN** 调用方两次调用 `device->GetQueue(QueueType::GRAPHICS)`
- **THEN** 返回的 `Queue*` 是同一指针（按 device 缓存）

#### Scenario: GLES 后端 compute / transfer 共享 queue
- **WHEN** 在 GLES 后端调用 `device->GetQueue(QueueType::GRAPHICS)`、`device->GetQueue(QueueType::COMPUTE)`、`device->GetQueue(QueueType::TRANSFER)`
- **THEN** 三次返回值可以是同一 `Queue*`，并且每个返回值 `GetType()` 报告对应的 `QueueType`（即 type 是查询参数而非 queue 真实属性）

### Requirement: Queue::Submit 提交命令缓冲

`Queue` SHALL 提供 `Submit(const SubmitInfo &info)` 方法，将 `info.commandBuffers` 中的所有命令按顺序提交到底层硬件队列执行。

`SubmitInfo` MUST 至少包含：
- `std::vector<CommandBuffer*> commandBuffers`（可为空，仅信号 / 等待）
- `std::vector<SemaphoreSubmitInfo> waitSemaphores`
- `std::vector<SemaphoreSubmitInfo> signalSemaphores`
- `Fence *fence`（可为 nullptr）

执行顺序：先在 `waitSemaphores` 全部满足后开始执行 `commandBuffers`，结束后按顺序 signal `signalSemaphores` 与（如有）`fence`。

调用方 MUST 在 `Submit` 返回后保持 `commandBuffers` 中的对象存活直到对应 fence 已 signaled 或对应 signalSemaphore 的 value 已被等待。

#### Scenario: 单 cmdbuf 提交无信号无 fence
- **WHEN** 调用方录制完一个 `CommandBuffer` 并构造仅含该 cmdbuf 的 `SubmitInfo`，调用 `queue->Submit(info)`
- **THEN** Submit 返回；之后调用 `queue->WaitIdle()` 后该 cmdbuf 的命令已在 GPU 上执行完毕

#### Scenario: Submit 携带 fence 完成查询
- **WHEN** 调用方传入未 signaled 的 `Fence*` 调用 Submit，并在 Submit 后立即调用 `fence->IsSignaled()`
- **THEN** 返回 `false`；调用 `fence->Wait()` 阻塞直至 GPU 完成；之后 `fence->IsSignaled()` 返回 `true`

#### Scenario: Submit signal binary semaphore 给后续 submit wait
- **WHEN** 调用方在第一次 Submit 中 signal binary semaphore S，在第二次 Submit 中 wait S
- **THEN** 第二次 Submit 的 cmdbuf 在第一次完成后才开始执行；可由 fence 或最终输出验证

#### Scenario: Submit 支持空 cmdbuf 列表用于纯信号传递
- **WHEN** 调用方构造 `commandBuffers` 为空但 `signalSemaphores` 非空的 SubmitInfo 调用 Submit
- **THEN** Submit 不报错，并在等待项满足后 signal 所有 signalSemaphores；fence（如有）也被 signal

### Requirement: Queue::WaitIdle 同步等待该队列空闲

`Queue` SHALL 提供 `WaitIdle()` 方法，阻塞调用线程直到该队列上**所有已提交命令**完成执行。

#### Scenario: WaitIdle 在 Submit 之后保证完成
- **WHEN** 调用方 Submit 一个 cmdbuf，紧接着调用 `queue->WaitIdle()`
- **THEN** WaitIdle 返回后，该 cmdbuf 中所有 GPU 命令都已完成（可由后续 buffer readback 验证）

### Requirement: Device::WaitIdle 等待全部队列

`Device::WaitIdle()` SHALL 在返回前等待该 device 上**所有 Queue** 的全部已提交命令完成（等价于对 graphics / compute / transfer 各自调用 WaitIdle，且不规定先后）。

#### Scenario: 多队列均空闲
- **WHEN** 多次向不同队列 Submit 后调用 `device->WaitIdle()`
- **THEN** 返回时所有队列上的命令均已完成
