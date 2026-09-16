## MODIFIED Requirements

### Requirement: StaticBuffer 委托 RHI 上传一次不可变

`StaticBuffer`（非模板，继承 `RenderResource`）SHALL 以 `MemoryType::GPU_ONLY` 创建底层 buffer，`Init(Device*, uint64_t size, BufferUsageFlags usage)` 传入大小与用途位；`Upload` SHALL 构造 `BufferUploadRequest` 并调 `Queue::UploadBuffer` 一次性上传数据，此后 SHALL 不可写；其 `Map()` SHALL 返回 `nullptr`。

#### Scenario: static buffer 不可映射

- **WHEN** `StaticBuffer` 完成上传后调用 `Map()`
- **THEN** 返回 `nullptr`（GPU_ONLY 不可 host 映射）

#### Scenario: 委托 RHI 上传数据

- **WHEN** 用 `Init(dev, size, INDEX)` 初始化后 `Upload(data, size)`
- **THEN** 上传经 `Queue::UploadBuffer` 完成；底层 buffer 创建成功且 usage 含 `INDEX`、memory 为 `GPU_ONLY`、size 与入参一致

### Requirement: DynamicBuffer CPU 上传并同步 inflight frame

`DynamicBuffer`（非模板，继承 `RenderResource`）SHALL 以 `MemoryType::CPU_TO_GPU` 创建底层 buffer，`Init(Device*, uint64_t size, BufferUsageFlags usage, uint32_t framesInFlight)` 传入大小、用途位与 in-flight 帧数；走 **CPU 上传**：`Map()` SHALL 返回持久可写指针，`Write(data, size, offset)` 每帧写入。在帧在途（inflight）期间覆盖写 SHALL 通过 ring/double-buffer 或 fence 同步，避免踩踏在途帧数据。

#### Scenario: 持久映射写入

- **WHEN** `DynamicBuffer` 完成惰性创建后调用 `Map()` 并向返回指针写入数据
- **THEN** 写入成功且底层 buffer memory 为 `CPU_TO_GPU`

#### Scenario: 按偏移写入

- **WHEN** 调用 `Write(data, size, offset)`（offset 非 0）
- **THEN** 数据写入到 `offset` 处，且 `offset + size ≤ 总 size` 不越界

#### Scenario: inflight frame 同步

- **WHEN** 连续多帧对同一 `DynamicBuffer` 每帧写入，且前一帧仍在途（GPU 未消费完）
- **THEN** 写入通过 ring/double-buffer 或 fence 等待，不覆盖在途帧数据

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

## ADDED Requirements

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

## REMOVED Requirements

### Requirement: buffer kind tag 声明用途位

### Requirement: 命名类型与 tier 组合可表达
