## ADDED Requirements

### Requirement: Semaphore 区分 binary 与 timeline 类型

`Semaphore::Descriptor` SHALL 包含 `SemaphoreType type`（默认 `BINARY`）与 `uint64_t initialValue`（仅 `TIMELINE` 使用，默认 0）。

`Semaphore` SHALL 提供 `GetType() -> SemaphoreType` 查询自身类型。

后端 MUST 按 type 选择底层实现：
- BINARY：Vulkan binary semaphore / DX12 ID3D12Fence（auto-incrementing）/ Metal MTLEvent / GLES 软实现
- TIMELINE：Vulkan timelineSemaphore / DX12 ID3D12Fence（caller-supplied value）/ Metal MTLSharedEvent / GLES 软实现（atomic + condvar）

#### Scenario: 创建 binary semaphore
- **WHEN** 调用 `device->CreateSema({.type = BINARY})`
- **THEN** 返回非空 `Semaphore*`，其 `GetType() == BINARY`

#### Scenario: 创建 timeline semaphore 带初始值
- **WHEN** 调用 `device->CreateSema({.type = TIMELINE, .initialValue = 5})`
- **THEN** 返回的 timeline semaphore 当前 host-side 可见 value 为 5

### Requirement: Timeline semaphore 支持 host-side signal 与 wait

对于 `TIMELINE` 类型的 `Semaphore`，SHALL 提供：
- `Signal(uint64_t value)`：host 端将 semaphore 推进到 `value`（要求 `value` 严格大于当前值）
- `Wait(uint64_t value, uint64_t timeoutNs) -> bool`：阻塞直到 semaphore 达到 ≥ `value`，或超时；超时返回 false
- `GetCurrentValue() -> uint64_t`：返回 host 当前可见的 value（轮询用）

对 `BINARY` semaphore 调用上述方法的行为是未定义（实现 MAY 通过 assert 触发错误）。

#### Scenario: host 端 signal 推进 timeline value
- **WHEN** 创建 initialValue=0 的 timeline semaphore，调用 `sema->Signal(10)`
- **THEN** `sema->GetCurrentValue() ≥ 10`；其它线程 `Wait(10, UINT64_MAX)` 立即返回 true

#### Scenario: timeline value 必须单调递增
- **WHEN** 调用 `Signal(5)` 后再 `Signal(3)`
- **THEN** 后端 MUST 拒绝（assert / 返回错误）；`GetCurrentValue()` 仍 ≥ 5

#### Scenario: Wait 超时返回 false
- **WHEN** 调用 `Wait(100, 1_000_000)`（1ms）但 semaphore 当前值仍是 0 且无人 signal
- **THEN** 阻塞约 1ms 后返回 false

### Requirement: SubmitInfo 中 SemaphoreSubmitInfo 携带 stage mask 与 value

`SemaphoreSubmitInfo` SHALL 包含：
- `Semaphore *semaphore`
- `uint64_t value`（仅 timeline 使用，binary 时被实现忽略）
- `PipelineStageFlags stageMask`（默认 `PipelineStageBit::TOP`，表示在该 stage 等待 / signal）

后端在转换为 native API 时 MUST 用 `stageMask` 对应到 `VkPipelineStage2` / `D3D12_BARRIER_SYNC` / `MTLRenderStages`。

#### Scenario: 跨队列 timeline 链
- **WHEN** queue A Submit signal `(timelineSema, value=10)`；queue B Submit wait `(timelineSema, value=10, stage=VERTEX_SHADER)`
- **THEN** queue B 的 cmdbuf 仅在 timelineSema 达到 10 后才在 vertex stage 开始执行；两次 Submit 之间无显式 host wait

### Requirement: Fence 提供非阻塞查询与重置

`Fence` SHALL 提供：
- `Wait()`（已有）：阻塞至 signaled
- `Reset()`（已有）：将 fence 置为 unsignaled
- `IsSignaled() -> bool`（**新增**）：非阻塞查询；signaled 返回 true，否则 false
- `WaitFor(uint64_t timeoutNs) -> bool`（**新增**）：带超时的 wait；超时返回 false

`Fence` MUST 仅与 `Queue::Submit` 配合使用（不能 host signal）。Submit 完成时 fence 被 signal；调用方 Reset 后可以复用。

#### Scenario: IsSignaled 在 Submit 完成前为 false
- **WHEN** Submit 一个含 cmdbuf 的 SubmitInfo 并立即 `fence->IsSignaled()`
- **THEN** 在 GPU 完成前返回 false；`fence->Wait()` 后 `IsSignaled()` 返回 true

#### Scenario: WaitFor 超时
- **WHEN** Submit 一个长任务并立即 `fence->WaitFor(0)`
- **THEN** 返回 false（任务尚未完成）；之后 `fence->Wait()` 阻塞至完成

#### Scenario: Reset 后复用
- **WHEN** Submit + `Wait()` 后调用 `fence->Reset()`，再次 Submit 同一 fence
- **THEN** `IsSignaled()` 在第二次 Submit 完成前为 false；`Wait()` 正常工作

### Requirement: Acquire 与 Present 必须用 binary semaphore

`SwapChain::AcquireNextImage` 的 `signalSema` 与 `SwapChain::Present` 的 `waitSemas` MUST 全部为 BINARY 类型。

后端在收到 timeline semaphore 时 MUST 拒绝（assert / 报错），因为大多数 native swapchain API 不接受 timeline semaphore。

#### Scenario: Present 拒绝 timeline semaphore
- **WHEN** 调用方误传 timeline semaphore 给 `swapchain->Present`
- **THEN** Debug build 下触发 assert；Release build 下行为未定义但 MUST 不静默成功
