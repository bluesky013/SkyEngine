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

### Requirement: buffer kind tag 声明用途位

`VertexBufferKind` / `IndexBufferKind` / `UniformBufferKind` / `StorageBufferKind` SHALL 各自静态声明其 `BufferUsageFlags`；其中 `StorageBufferKind` 的 usage SHALL 为 `STORAGE | VERTEX | INDEX | INDIRECT` 超集。tier 模板 SHALL 用该 `Kind::Usage` 创建底层 buffer。

#### Scenario: kind 映射到 usage

- **WHEN** 用 `VertexBufferKind` 实例化任一 tier 模板并创建底层 buffer
- **THEN** 底层 `rhi::Buffer::Descriptor.usage` 含 `VERTEX`；`IndexBufferKind`→`INDEX`、`UniformBufferKind`→`UNIFORM`；`StorageBufferKind`→同时含 `STORAGE`、`VERTEX`、`INDEX`、`INDIRECT`

### Requirement: StaticBuffer 委托 RHI 上传一次不可变

`template <typename Kind> StaticBuffer` SHALL 以 `MemoryType::GPU_ONLY` 创建底层 buffer，`Upload` SHALL 构造 `BufferUploadRequest` 并调 `Queue::UploadBuffer` 一次性上传数据，此后 SHALL 不可写；其 `Map()` SHALL 返回 `nullptr`。

#### Scenario: static buffer 不可映射

- **WHEN** `StaticBuffer<VertexBufferKind>` 完成上传后调用 `Map()`
- **THEN** 返回 `nullptr`（GPU_ONLY 不可 host 映射）

#### Scenario: 委托 RHI 上传数据

- **WHEN** 用 `Upload(data, size)` 上传 `StaticBuffer<IndexBufferKind>`
- **THEN** 上传经 `Queue::UploadBuffer` 完成（staging vs 直接写由 RHI 按 isUMA 决定）；底层 buffer 创建成功且 usage 含 `INDEX`、memory 为 `GPU_ONLY`、size 与入参一致

### Requirement: DynamicBuffer CPU 上传并同步 inflight frame

`template <typename Kind> DynamicBuffer` SHALL 以 `MemoryType::CPU_TO_GPU` 创建底层 buffer，走 **CPU 上传**：`Map()` SHALL 返回持久可写指针，`Write(data, size, offset)` 每帧写入。在帧在途（inflight）期间覆盖写 SHALL 通过 ring/double-buffer 或 fence 同步，避免踩踏在途帧数据。

#### Scenario: 持久映射写入

- **WHEN** `DynamicBuffer<UniformBufferKind>` 完成惰性创建后调用 `Map()` 并向返回指针写入数据
- **THEN** 写入成功且底层 buffer memory 为 `CPU_TO_GPU`、usage 含 `UNIFORM`

#### Scenario: 按偏移写入

- **WHEN** 调用 `Write(data, size, offset)`（offset 非 0）
- **THEN** 数据写入到 `offset` 处，且 `offset + size ≤ 总 size` 不越界

#### Scenario: inflight frame 同步

- **WHEN** 连续多帧对同一 `DynamicBuffer` 每帧写入，且前一帧仍在途（GPU 未消费完）
- **THEN** 写入通过 ring/double-buffer 或 fence 等待，不覆盖在途帧数据

### Requirement: TransientBuffer 每帧分配（与 RDG TransientPool 区分）

`template <typename Kind> TransientBuffer` SHALL 从 `TransientBufferPool` 每帧分配临时 buffer，仅当前帧有效；池 SHALL 对齐 `Device::capability.minUniformBufferOffsetAlignment`，帧末 `Reset()` 回收。`TransientBuffer`/`TransientBufferPool` SHALL NOT 复用 RDG 的 `TransientPool`——前者是每帧内使用的 buffer 资源，后者是 RDG 每帧可复用的资源池。

#### Scenario: 每帧分配与对齐

- **WHEN** 连续两帧各分配一个同 descriptor 的 `TransientBuffer<StorageBufferKind>`，并在帧末调用 `Reset()`
- **THEN** 两帧均从 `TransientBufferPool` 分配，分配的偏移按 `minUniformBufferOffsetAlignment` 对齐，且不依赖 RDG `TransientPool`

### Requirement: VertexBuffer 携带顶点布局元数据

`VertexBuffer`（默认 `StaticBuffer<VertexBufferKind>`）SHALL 携带 `VertexLayout`（`stride` + `VertexSemanticMask` 语义 + `VertexInputRate`），并提供 `GetLayout()`，供 `BindVertexBuffers` 使用。

#### Scenario: 顶点布局可查询

- **WHEN** 创建 `VertexBuffer` 并设置 `VertexLayout{stride=32, semantics={POSITION,NORMAL}}`
- **THEN** `GetLayout().stride == 32`，语义 mask 含 `POSITION` 与 `NORMAL`；`GetBuffer()` 返回非空底层 buffer

### Requirement: IndexBuffer 携带索引类型元数据

`IndexBuffer`（默认 `StaticBuffer<IndexBufferKind>`）SHALL 携带 `rhi::IndexType`（`U16`/`U32`），并提供 `GetIndexType()`，供 `BindIndexBuffer` 使用。

#### Scenario: 索引类型可查询

- **WHEN** 创建 `IndexBuffer` 并设 `IndexType::U32`
- **THEN** `GetIndexType() == U32`；`GetBuffer()` 返回非空底层 buffer

### Requirement: StorageBuffer 是可重解释的通用视图

`StorageBuffer`（默认 `DynamicBuffer<StorageBufferKind>`）SHALL 作为通用 buffer：可作 SSBO 绑定，且 SHALL 提供 `AsVertex`/`AsIndex`/`AsIndirect` 重解释（返回底层 `rhi::Buffer*`），使同一块底层 buffer 可被重新绑定为 vertex / index / indirect buffer。重解释 SHALL 不隐含任何 barrier；调用方 SHALL 自行插入所需 barrier。

#### Scenario: CS 输出重解释为 vertex/index/indirect

- **WHEN** 一个 `StorageBuffer` 作为 CS 结果写入后，调用 `AsVertex(offset)` / `AsIndex(offset)` / `AsIndirect(offset)`
- **THEN** 各返回指向同一底层 buffer 的非空 `rhi::Buffer*`；本层不自动插入 barrier

### Requirement: 命名类型与 tier 组合可表达

模块 SHALL 提供 `VertexBuffer`/`IndexBuffer`/`UniformBuffer`/`StorageBuffer` 作为模板化于 storage tier 的类型（带默认 tier：Vertex/Index 默认 `StaticBuffer`，Uniform/Storage 默认 `DynamicBuffer`）。任意 `tier × kind` 组合 SHALL 可通过显式模板实参表达（如 `VertexBuffer<DynamicBuffer<VertexBufferKind>>`）。

#### Scenario: 动态顶点缓冲可表达

- **WHEN** 声明 `VertexBuffer<DynamicBuffer<VertexBufferKind>> vb;`
- **THEN** `vb` 编译通过，为 dynamic/vertex 语义，且 `GetLayout()` 仍可用

### Requirement: 上传依赖 RHI 上传能力（aurora-upload）

本 change 的上传 SHALL 依赖 `aurora-upload` 能力（`DeviceCapability::isUMA` + `Queue::UploadBuffer`/`UploadImage`）；resource 层 SHALL NOT 自行实现 staging 或 `isUMA` 分支。

#### Scenario: 上传委托 RHI

- **WHEN** 编译 `aurora/resource` 头文件并调用任一 buffer 的 `Upload`
- **THEN** 上传经 `Queue::UploadBuffer` 委托 RHI；`aurora/resource` 不含 staging buffer 分配与 `isUMA` 判断

