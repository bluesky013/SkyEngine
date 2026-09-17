# aurora-render-buffers Specification

## Purpose
TBD - created by archiving change aurora-render-buffers. Update Purpose after archive.
## Requirements
### Requirement: RenderResource 是 buffer/image 的公共资源封装基类

`RenderResource` SHALL 作为 buffer 与 image 资源的统一非模板基类，作用是对 rhi 资源的一次封装，提供：

- **惰性创建**：构造时仅持有 `rhi::Device*` 与（`SKY_ENABLE_RESOURCE_NAME == 1` 时的）`Name`，底层 `rhi::Buffer`/`rhi::Image` SHALL 在首次访问（`Create()`）时才创建；`IsCreated()` SHALL 反映是否已创建。
- **统一上传**：`Upload(const void* data, uint64_t size, uint64_t offset = 0)` SHALL 把 host 数据上传到 device 资源，上传策略（staging vs 直接写）SHALL 委托 RHI（见 `aurora-upload` spec），resource 层 SHALL NOT 内含 staging 逻辑。

`RenderResource` SHALL NOT 引入或持有 `rhi::BufferView`，也 SHALL NOT 持有 `offset/range` 视图字段。`name_` SHALL 仅在 `SKY_ENABLE_RESOURCE_NAME == 1` 时存在（见 `aurora-resource-name` spec），`Create()` 时 SHALL 把名字下传 `Descriptor::name`。

#### Scenario: 惰性创建

- **WHEN** 构造一个 buffer 资源但尚未访问其底层 buffer
- **THEN** `IsCreated() == false`，底层 `rhi::Buffer` 尚未创建；首次 `Upload`/访问后 `IsCreated() == true`

#### Scenario: 统一上传入口

- **WHEN** 对一个 buffer 资源调用 `Upload(data, size, offset)`
- **THEN** host 数据被上传到 device 资源指定偏移处，且该资源完成惰性创建

### Requirement: 资源/显存统计归后端

资源/显存统计（实际创建了多少 buffer/image、按类别占用多少显存）SHALL 由后端负责——Vulkan 用 VMA、DX12 用 D3D12MA 做显存池，统计只有后端知道。resource 层 SHALL NOT 自建 `RenderResourceManager`（登记表 + 计数 + 内存统计）。

#### Scenario: resource 层不重复统计

- **WHEN** 阅读 `aurora/resource` 的实现
- **THEN** 无自建的资源登记表/内存统计；统计由后端 allocator（VMA/D3D12MA）提供

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

### Requirement: TransientBuffer 每帧分配（与 RDG TransientPool 区分）

`TransientBuffer`（非模板，继承 `RenderResource`）SHALL 是每帧内使用的临时 buffer，`Init(Device*, uint64_t size, BufferUsageFlags usage)`；仅当前帧有效。`TransientBuffer` SHALL NOT 复用 RDG 的 `TransientPool`——前者是每帧内使用的 buffer 资源，后者是 RDG 每帧可复用的资源池。

#### Scenario: 与 RDG TransientPool 区分

- **WHEN** 阅读 `TransientBuffer` 的实现
- **THEN** 不依赖 RDG `TransientPool`（v1 直接经 `Device` 分配，专用 `TransientBufferPool` 是 follow-up）

### Requirement: VertexBuffer 携带顶点布局元数据

`VertexBuffer`（非模板，继承 `StaticBuffer`）SHALL 携带 `VertexLayout`（`stride` + `VertexSemanticMask` 语义 + `VertexInputRate`），并提供 `GetLayout()`/`SetLayout()`，供 `BindVertexBuffers` 使用。`Init(Device*, uint64_t size)` SHALL 以 `VERTEX` 用途位初始化。

#### Scenario: 顶点布局可查询

- **WHEN** 创建 `VertexBuffer` 并设置 `VertexLayout{stride=32, semantics={POSITION,NORMAL}}`
- **THEN** `GetLayout().stride == 32`，语义 mask 含 `POSITION` 与 `NORMAL`；`GetBuffer()` 返回非空底层 buffer

### Requirement: IndexBuffer 携带索引类型元数据

`IndexBuffer`（非模板，继承 `StaticBuffer`）SHALL 携带 `rhi::IndexType`（`U16`/`U32`），并提供 `GetIndexType()`/`SetIndexType()`，供 `BindIndexBuffer` 使用。`Init(Device*, uint64_t size)` SHALL 以 `INDEX` 用途位初始化。

#### Scenario: 索引类型可查询

- **WHEN** 创建 `IndexBuffer` 并设 `IndexType::U32`
- **THEN** `GetIndexType() == U32`；`GetBuffer()` 返回非空底层 buffer

### Requirement: StorageBuffer 是可重解释的通用视图

`StorageBuffer`（非模板，继承 `DynamicBuffer`）SHALL 作为通用 buffer：可作 SSBO 绑定，且 SHALL 提供 `AsVertex`/`AsIndex`/`AsIndirect` 重解释（返回底层 `rhi::Buffer*`），使同一块底层 buffer 可被重新绑定为 vertex / index / indirect buffer。重解释 SHALL 不隐含任何 barrier；调用方 SHALL 自行插入所需 barrier。

#### Scenario: CS 输出重解释为 vertex/index/indirect

- **WHEN** 一个 `StorageBuffer` 作为 CS 结果写入后，调用 `AsVertex(offset)` / `AsIndex(offset)` / `AsIndirect(offset)`
- **THEN** 各返回指向同一底层 buffer 的非空 `rhi::Buffer*`；本层不自动插入 barrier

### Requirement: 上传依赖 RHI 上传能力（aurora-upload）

本 change 的上传 SHALL 依赖 `aurora-upload` 能力（`DeviceCapability::isUMA` + `Queue::UploadBuffer`/`UploadImage`）；resource 层 SHALL NOT 自行实现 staging 或 `isUMA` 分支。

#### Scenario: 上传委托 RHI

- **WHEN** 编译 `aurora/resource` 头文件并调用任一 buffer 的 `Upload`
- **THEN** 上传经 `Queue::UploadBuffer` 委托 RHI；`aurora/resource` 不含 staging buffer 分配与 `isUMA` 判断

### Requirement: buffer 用途位由具体类型 Init 固定

`VertexBuffer`/`IndexBuffer`/`UniformBuffer`/`StorageBuffer` SHALL 作为非模板具体类型提供，各自 `Init` SHALL 固定用途位：

- `VertexBuffer` → `VERTEX`
- `IndexBuffer` → `INDEX`
- `UniformBuffer` → `UNIFORM`
- `StorageBuffer` → `STORAGE | VERTEX | INDEX | INDIRECT`

模块 SHALL NOT 提供 kind tag 结构体（`VertexBufferKind` 等）或「tier × kind」模板组合。

#### Scenario: 用途位固定

- **WHEN** 用 `VertexBuffer::Init(dev, size)` 创建底层 buffer
- **THEN** 底层 `rhi::Buffer::Descriptor.usage` 含 `VERTEX`；`IndexBuffer`→`INDEX`、`UniformBuffer`→`UNIFORM`、`StorageBuffer`→同时含 `STORAGE`/`VERTEX`/`INDEX`/`INDIRECT`

#### Scenario: 无 kind tag 与 tier 模板

- **WHEN** 阅读 `Buffer.h`
- **THEN** 无 `VertexBufferKind`/`IndexBufferKind`/`UniformBufferKind`/`StorageBufferKind`，`StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 与四个具体类型均为非模板类

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

