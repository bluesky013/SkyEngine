## MODIFIED Requirements

### Requirement: StaticBuffer 委托 RHI 上传一次不可变

`StaticBuffer`（非模板，继承 `RenderResource`）SHALL 以 `MemoryType::GPU_ONLY` 创建底层 buffer，`Init(Device*, uint64_t size, BufferUsageFlags usage)` 传入大小与用途位；`Upload` SHALL 构造 `BufferUploadRequest` 并调 `Queue::UploadBuffer` 一次性**异步**上传数据，此后 SHALL 不可写；其 `Map()` SHALL 返回 `nullptr`。

`Upload` SHALL 追踪返回的 `TransferTaskHandle` 与提交它的 `Queue*`，并提供 `IsUploadComplete()`（返回是否完成）与 `WaitUploadComplete()`（阻塞等待完成，完成后清空 pending）。析构（destructor）SHALL 在释放底层 buffer 前等待 pending 上传完成，避免在途销毁。

#### Scenario: static buffer 不可映射

- **WHEN** `StaticBuffer` 完成上传后调用 `Map()`
- **THEN** 返回 `nullptr`（GPU_ONLY 不可 host 映射）

#### Scenario: 委托 RHI 异步上传数据

- **WHEN** 用 `Init(dev, size, INDEX)` 初始化后 `Upload(data, size)`
- **THEN** 上传经 `Queue::UploadBuffer` 异步完成；底层 buffer 创建成功且 usage 含 `INDEX`、memory 为 `GPU_ONLY`、size 与入参一致

#### Scenario: 完成语义可查询

- **WHEN** `Upload` 返回后立即调 `IsUploadComplete()`
- **THEN** 返回 false 或 true（取决于 transfer 队列进度）；`WaitUploadComplete()` 返回后 `IsUploadComplete() == true`

#### Scenario: 析构前等待 pending

- **WHEN** `Upload` 后立即析构该 `StaticBuffer`
- **THEN** 析构（destructor）等待 pending 上传完成后再释放底层 buffer，不产生 `vkDestroyBuffer` 在途使用校验错误

### Requirement: DynamicBuffer CPU 上传并同步 inflight frame

`DynamicBuffer`（非模板，继承 `RenderResource`）SHALL 以 `MemoryType::CPU_TO_GPU` 创建底层 buffer，`Init(Device*, uint64_t size, BufferUsageFlags usage, uint32_t framesInFlight)` 传入大小、用途位与 in-flight 帧数；走 **当帧直写**：`Map()` SHALL 返回持久可写指针，`Write(data, size, offset)` 每帧写入当前 frame 的映射内存。

在帧在途（inflight）期间覆盖写 SHALL 通过 ring 同步避免踩踏在途帧数据。ring 槽位 SHALL 与 `DeviceFrameContext` 对齐：`framesInFlight` SHALL 等于 frame context 的 `inflightNum`；frame driver SHALL 每帧在 `DeviceFrameContext::BeginFrame()`（已对当前槽 fence `Wait+Reset`）之后调 `AdvanceFrame()`，使 `current` 与 `mFrameIndex` 同锁步。in-flight 安全由 frame context 的 fence 保证，buffer SHALL NOT 自建 fence。

#### Scenario: 持久映射写入

- **WHEN** `DynamicBuffer` 完成惰性创建后调用 `Map()` 并向返回指针写入数据
- **THEN** 写入成功且底层 buffer memory 为 `CPU_TO_GPU`

#### Scenario: 按偏移写入

- **WHEN** 调用 `Write(data, size, offset)`（offset 非 0）
- **THEN** 数据写入到 `offset` 处，且 `offset + size ≤ 总 size` 不越界

#### Scenario: inflight frame 同步

- **WHEN** 连续多帧对同一 `DynamicBuffer` 每帧写入，且前一帧仍在途（GPU 未消费完）
- **THEN** 写入通过 ring 轮换不覆盖在途帧数据

#### Scenario: ring 与 frame context 锁步

- **WHEN** `framesInFlight == inflightNum` 且 frame driver 每帧在 `BeginFrame()` 后调 `AdvanceFrame()`
- **THEN** `current` 与 `DeviceFrameContext::mFrameIndex` 同锁步；当前槽的 fence 已在 `BeginFrame()` 中 `Wait`，写入安全

## ADDED Requirements

### Requirement: resource 层上传分两种模式

resource 层上传 SHALL 明确分为两种模式：

- **异步上传（transfer queue + staging）**：`StaticBuffer` / `Texture`（GPU_ONLY）→ `Queue::UploadBuffer`/`UploadImage`，异步非阻塞，`TransferTaskHandle` 同步。
- **当帧 staging 上传（frame context）**：`FrameStagingBuffer` → `StagingBufferAllocator` 每帧 bump + 内联 `BlitEncoder::CopyBuffer`。

`DynamicBuffer`/`TransientBuffer` 的 CPU_TO_GPU 持久映射直写 SHALL NOT 属于上述两种上传模式（是第三种机制，无 copy）。

#### Scenario: 两种模式划分

- **WHEN** 阅读 `aurora/resource` 的上传实现
- **THEN** `StaticBuffer`/`Texture` 走异步 transfer queue 上传；`FrameStagingBuffer` 走当帧 staging 上传；`DynamicBuffer`/`TransientBuffer` 走持久映射直写（非上传模式）

### Requirement: FrameStagingBuffer 当帧 staging 上传

`FrameStagingBuffer`（resource 层，`aurora/resource/FrameStagingBuffer.h`）SHALL 包装 `StagingBufferAllocator`，提供当帧 staging 上传：

- `Init(Device*, uint64_t segmentSize, uint32_t numFrames)`：`numFrames` SHALL 等于 `DeviceFrameContext::inflightNum`；
- `Upload(Buffer *dst, const void *data, uint64_t size, uint64_t dstOffset = 0)`：写入当前帧 staging 槽并排队 pending copy；
- `Flush(BlitEncoder &encoder)`：在当前帧 command buffer 内联 `CopyBuffer` 到目标，清空队列；
- `Reset()`：随 frame context 每帧推进到下一 in-flight 段。

in-flight 安全由 `StagingBufferAllocator` 的 per-frame ring 保证。`FrameStagingBuffer` SHALL NOT 内含 `isUMA` 分支或自建 fence。

#### Scenario: staging 上传并内联 copy

- **WHEN** `Upload(dst, data, size)` 后在一个 command buffer 的 COPYBLIT pass 内调 `Flush(encoder)`
- **THEN** 数据被内联 `CopyBuffer` 到 `dst` 指定偏移；提交并等待后 `dst` 读回 data

#### Scenario: staging 段耗尽

- **WHEN** 当帧 staging 段剩余空间不足
- **THEN** `Upload` 返回 false，不越界写入
