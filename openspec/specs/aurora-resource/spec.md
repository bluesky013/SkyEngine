# aurora-resource Specification

## Purpose
TBD - consolidated from: aurora-render-buffers aurora-render-geometry aurora-render-textures aurora-upload
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


### Requirement: RenderGeometry 是复合网格资源（纯封装）

`RenderGeometry` SHALL 是一个 `RefObject` 复合资源（**不**继承 `RenderResource`），持有：顶点流 `std::vector<std::unique_ptr<VertexBuffer<>>>`（每个自带 `VertexLayout`）、可选索引缓冲 `std::unique_ptr<IndexBuffer<>>`（自带 `IndexType`）、本地 `AABB localBounds`。

`RenderGeometry` SHALL 提供 `AddVertexStream(std::unique_ptr<VertexBuffer<>>)` / `SetIndexBuffer(std::unique_ptr<IndexBuffer<>>)` 转移所有权，以及 `GetVertexStreams()` / `GetIndexBuffer()` / `GetLocalBounds()` / `GetName()` 访问器。

`RenderGeometry` SHALL 是**纯封装**：SHALL NOT 持有 host 数据、SHALL NOT 提供 `Upload()`（上传是调用方 + buffer 自身职责）。

#### Scenario: 组合顶点流 + 索引 + 包围盒

- **WHEN** 创建一个 `RenderGeometry`，`AddVertexStream` 一个 `VertexBuffer`、`SetIndexBuffer` 一个 `IndexBuffer`、`SetLocalBounds` 一个 `AABB`
- **THEN** `GetVertexStreams().size() == 1`、`GetIndexBuffer()` 非空、`GetLocalBounds()` 等于所设值

#### Scenario: 不继承 RenderResource

- **WHEN** 阅读 `RenderGeometry` 的类层次
- **THEN** 它继承 `RefObject`，不继承 `RenderResource`（它是复合资源，不是单个 rhi 资源封装）

#### Scenario: 无 Upload 方法

- **WHEN** 阅读 `RenderGeometry` 的接口
- **THEN** 无 `Upload()` 方法（buffer 上传由 `VertexBuffer`/`IndexBuffer` 自身完成，geometry 不参与）

### Requirement: unique_ptr 独占所有权（不造 Ptr 别名）

`RenderGeometry` SHALL 通过 `std::unique_ptr<VertexBuffer<>>` / `std::unique_ptr<IndexBuffer<>>` 独占持有其 buffer，析构时释放底层 buffer。模块 SHALL NOT 为独占所有权引入 `XxxPtr` 别名（仓库 `XxxPtr` 约定是 `CounterPtr<Xxx>` 共享语义）。

#### Scenario: geometry 拥有 buffer 生命周期

- **WHEN** `RenderGeometry` 析构
- **THEN** 其持有的 `unique_ptr` 被释放，底层 `rhi::Buffer` 引用计数归零

#### Scenario: 不引入 Ptr 别名

- **WHEN** 阅读 `RenderGeometry.h`
- **THEN** 未定义 `VertexBufferPtr` / `IndexBufferPtr`（独占所有权直接用 `std::unique_ptr` 表达）

### Requirement: 包围盒为本地空间 AABB

`RenderGeometry` 的 `localBounds` SHALL 是模型/本地空间的 `AABB`；世界空间包围盒由 scene 的 `WorldInfo` 变换推导，SHALL NOT 在 geometry 内重复存储。

#### Scenario: 本地包围盒可查询

- **WHEN** 设置 `localBounds` 为某 `AABB{min, max}` 后调用 `GetLocalBounds()`
- **THEN** 返回值与该 `AABB` 一致


### Requirement: Texture 是 image 侧的 RenderResource 子类（惰性创建 + 统一上传）

`Texture` SHALL 继承 `RenderResource`（非模板），作为 image 资源的封装：持有 `rhi::Image::Descriptor` 与 `ImagePtr`，惰性 `Create()` 调 `Device::CreateImage(desc)`，`IsCreated()` SHALL 反映是否已创建。

`Texture` SHALL 提供统一上传：
- `Upload(data, size, offset)` SHALL 把 `data` 当作整张 **mip0、layer0 紧密打包** 数据上传（`offset` 为**源字节偏移**），构造单个 `ImageUploadRequest{mipLevel=0, layer=0, imageExtent=full extent}` 并转发 `UploadImage`。
- `UploadImage(std::vector<ImageUploadRequest>)` SHALL 委托 `Queue::UploadImage`，SHALL NOT 内含 staging / `isUMA` / in-flight 逻辑（见 `aurora-upload` spec）。

`Texture` 的上传 SHALL 为**异步**：`UploadImage` SHALL 追踪返回的 `TransferTaskHandle` 与提交它的 `Queue*`，提供 `IsUploadComplete()` 与 `WaitUploadComplete()`（阻塞等待并清空 pending）；析构（destructor）SHALL 在释放底层 image 前等待 pending 上传完成，避免在途销毁。

`Texture` SHALL 提供访问器 `GetImage()` / `GetDescriptor()` / `GetExtent()` / `GetMipLevels()` / `GetArrayLayers()` / `GetFormat()`。

#### Scenario: 惰性创建

- **WHEN** 构造一个 `Texture2D` 但尚未访问其底层 image
- **THEN** `IsCreated() == false`、`GetImage() == nullptr`；首次 `Upload`/`UploadImage` 后 `IsCreated() == true`、`GetImage()` 非空

#### Scenario: 上传委托 RHI

- **WHEN** 阅读 `Texture` 的上传实现
- **THEN** 上传经 `Queue::UploadImage` 委托 RHI；`Texture.h` 不含 staging buffer 分配、`isUMA` 判断与 in-flight 判定

#### Scenario: 便捷上传只覆盖 mip0、layer0

- **WHEN** 对一个 `Texture2D` 调 `Upload(data, size)`
- **THEN** 生成的 `ImageUploadRequest` 的 `mipLevel == 0`、`layer == 0`、`imageExtent` 等于整张纹理 extent

#### Scenario: 多 layer 纹理不走便捷上传

- **WHEN** 需要上传 `TextureCube`（6 面）或 `Texture2DArray`（N 层）的全部 layer
- **THEN** 调用方 SHALL 用 `UploadImage` 传每个 layer 的请求（便捷 `Upload` 只覆盖 layer0）

#### Scenario: 完成语义可查询

- **WHEN** `Upload`/`UploadImage` 返回后调 `WaitUploadComplete()`
- **THEN** 阻塞至 pending 上传完成，`IsUploadComplete() == true`

#### Scenario: 析构前等待 pending

- **WHEN** `Upload` 后立即析构该 `Texture`
- **THEN** 析构（destructor）等待 pending 上传完成后再释放底层 image，不产生 `vkDestroyImage` 在途使用校验错误

### Requirement: 维度特化类型的默认值

`Texture2D`/`TextureCube`/`Texture2DArray`/`Texture3D` SHALL 继承 `Texture`，各自提供维度便利 `Init(Device*, PixelFormat, extent, ...)`，并 SHALL 设置正确的维度默认值：

- `Texture2D`：`imageType == IMAGE_2D`，`arrayLayers == 1`；
- `TextureCube`：`imageType == IMAGE_2D`，强制 `arrayLayers == 6` 且 `viewUsage` 含 `CUBE_MAP_COMPATIBLE`；
- `Texture2DArray`：`imageType == IMAGE_2D`，`arrayLayers == 入参`；
- `Texture3D`：`imageType == IMAGE_3D`，`arrayLayers == 1`，`extent.depth` 为 3D 深度。

各维度类型默认 `usage` SHALL 含 `SAMPLED | TRANSFER_DST`。

#### Scenario: cube 默认 6 layer + cube-compatible

- **WHEN** `TextureCube::Init(dev, format, {w,h})` 后读 `GetDescriptor()` 与 `GetArrayLayers()`
- **THEN** `GetArrayLayers() == 6`，`desc.viewUsage` 含 `CUBE_MAP_COMPATIBLE`

#### Scenario: 2D array 用户给定 layer 数

- **WHEN** `Texture2DArray::Init(dev, format, {w,h}, 8)` 后读 `GetArrayLayers()`
- **THEN** `GetArrayLayers() == 8`

#### Scenario: 2D 单层

- **WHEN** `Texture2D::Init(dev, format, {w,h})` 后读 `GetArrayLayers()`
- **THEN** `GetArrayLayers() == 1`

### Requirement: v1 纹理一律 GPU_ONLY

`Texture::Init` SHALL 强制 `desc.memory = MemoryType::GPU_ONLY`。v1 SHALL NOT 提供纹理的 dynamic/transient tier；`RenderTarget`/`StorageTexture`/`TransientTexture` SHALL NOT 在本能力内实现。

#### Scenario: 纹理内存为 GPU_ONLY

- **WHEN** 任一维度 `Texture` 完成 `Init` 后读 `GetDescriptor()`
- **THEN** `desc.memory == MemoryType::GPU_ONLY`

### Requirement: TextureAtlas 是 2D 纹理上的 allocator 打包器

`TextureAtlas` SHALL 继承 `Texture2D`，持有一个打包分配器 `std::unique_ptr<TextureAtlasAllocator>`，并提供：

- `Allocate(uint32_t w, uint32_t h)` → `Result<Page>`：委托 allocator 分配一个 texel 空间子区域 `Page{x,y,w,h}`（装不下时 `Result` 的 bool 为 false）；
- `Upload(const Page&, const void* data, uint64_t size)`：把 `data`（紧密打包）上传到 `page` 对应子区域（`imageOffset={page.x,page.y,0}`、`imageExtent={page.w,page.h,1}`）；
- `SetAllocator(std::unique_ptr<TextureAtlasAllocator>)`：注入自定义打包器。

`TextureAtlasAllocator` SHALL 是抽象打包器接口，`TextureLinearAllocator` SHALL 是其 left→right/top→bottom 线性打包实现（永不释放）。`Init` SHALL 默认安装 `TextureLinearAllocator`。

#### Scenario: 分配子区域并上传

- **WHEN** `TextureAtlas::Init(dev, format, {64,64})` 后 `Allocate(16,16)` 得到 `Page p`，再 `Upload(p, data, size)`
- **THEN** `p` 位于 atlas 内（`p.x+p.w <= 64` 且 `p.y+p.h <= 64`）；上传经 `UploadImage` 到 `{p.x,p.y}` 处、extent `{16,16}`

#### Scenario: 连续分配不重叠且不越界

- **WHEN** 依次 `Allocate(32,32)`、`Allocate(32,32)`、`Allocate(32,32)`
- **THEN** 前两个成功且不重叠，第三个（超出 64 高度）返回 `Result` 的 bool 为 false

#### Scenario: 可注入自定义打包器

- **WHEN** 调 `SetAllocator(std::make_unique<TextureLinearAllocator>(w,h))`
- **THEN** 后续 `Allocate` 由注入的 allocator 处理


### Requirement: DeviceCapability 暴露 isUMA 内存拓扑

`DeviceCapability` SHALL 新增 `bool isUMA`（默认 `false`），后端 SHALL 在 `UpdateDeviceCaps()` 填充：

- Vulkan：存在 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_COHERENT_BIT` 的 memory heap 时 SHALL 置 `true`；
- DX12：`D3D12_FEATURE_DATA_ARCHITECTURE` 的 `UMA` / `CacheCoherentUMA` 为真时 SHALL 置 `true`；
- Metal：`MTLDevice.hasUnifiedMemory` 为真时 SHALL 置 `true`。

#### Scenario: 集成 GPU 报 UMA

- **WHEN** 在统一内存架构（Apple Silicon / 移动 SoC / Intel 集显）上初始化 Device
- **THEN** `GetCapability().isUMA == true`

#### Scenario: 独显报非 UMA

- **WHEN** 在独立显存 GPU（NVIDIA/AMD 独显）上初始化 Device
- **THEN** `GetCapability().isUMA == false`

### Requirement: Queue 提供 buffer/image 上传入口

`Queue` SHALL 提供 `UploadBuffer(const BufferPtr&, const std::vector<BufferUploadRequest>&)` 与 `UploadImage(const ImagePtr&, const std::vector<ImageUploadRequest>&)`，复用现有 `IUploadStream` / `BufferUploadRequest` / `ImageUploadRequest`，返回可等待句柄。

#### Scenario: buffer 上传

- **WHEN** 构造 `BufferUploadRequest`（含 `IUploadStream` 源 + offset/size）并调 `Queue::UploadBuffer`
- **THEN** 目标 `rhi::Buffer` 在请求 offset/size 处被写入源数据

### Requirement: per-frame staging ring 按 in-flight frame 回收

上传实现 SHALL 使用 per-frame staging ring（`CPU_TO_GPU`），SHALL 按 in-flight frame 分段，SHALL 只在对应 frame 的 fence 完成后才回收复用，保证写入区不与 GPU 正在读的区重叠。

#### Scenario: staging ring 不覆盖在途数据

- **WHEN** 连续多帧上传，前一帧的 copy 仍在途（fence 未完成）
- **THEN** 后续帧的 staging 分配不落入前一帧仍被 GPU 读取的区段

### Requirement: 上传路径按内存拓扑 × in-flight 状态选择

上传实现 SHALL 按以下规则选择路径：

- 目标**不可 host 直写**（discrete `GPU_ONLY`）→ 一律 staging ring + `CopyBuffer`；
- 目标**host 可见**且**不在途** → 直接 map + memcpy（无 copy）；
- 目标**host 可见**且**在途**（GPU 正在读）→ staging ring + copy（或 fence 等待后直写），避免 CPU/GPU 竞争。

#### Scenario: discrete 走 staging

- **WHEN** 在 `isUMA == false` 的设备上上传一块 `GPU_ONLY` buffer
- **THEN** 数据先写入 staging ring，再 copy 到目标（不直接 map 目标）

#### Scenario: UMA 非在途走直写

- **WHEN** 在 `isUMA == true` 的设备上上传一块「尚未被任何 command 引用」的 host-visible buffer
- **THEN** 直接 map + memcpy，无 staging copy

#### Scenario: UMA 在途走 staging

- **WHEN** 在 `isUMA == true` 的设备上上传一块「被上一帧在途 command 引用」的 host-visible buffer
- **THEN** 走 staging ring + copy（或 fence 等待后直写），不直接覆盖 GPU 正在读的内存

### Requirement: 两种上传模式（异步 upload queue vs 当帧 inline）

`Queue::UploadBuffer` / `UploadImage` SHALL 为**异步**上传：SHALL NOT 阻塞调用线程，SHALL 提交到 transfer/upload 队列并返回 `TransferTaskHandle`；`Queue::Wait(handle)` SHALL 阻塞等待完成，`Queue::HasComplete(handle)` SHALL 返回是否完成。当帧上传 SHALL 经 `StagingBufferAllocator`（每帧 bump 分配，`Allocate(size, align)` 返回 `{buffer, offset, mapped}`），调用方 SHALL 写入后经 `BlitEncoder::CopyBuffer` 内联记录在当前帧 command buffer。

#### Scenario: 异步上传不阻塞

- **WHEN** 调用 `UploadBuffer` 后立即返回
- **THEN** 返回非阻塞 `TransferTaskHandle`，数据在 transfer 队列上后台完成；`HasComplete` 在完成前为 false，`Wait` 后为 true

#### Scenario: 当帧上传内联可见

- **WHEN** 通过 `StagingBufferAllocator` 写入并内联 `CopyBuffer` 到当前帧 command buffer
- **THEN** 同一 command buffer 内的后续绘制可读到该数据（无跨队列同步）

### Requirement: resource 层上传委托 RHI

resource 层（`aurora-render-buffers` 的 `RenderResource::Upload` / `StaticBuffer::Upload`）SHALL 只构造 `BufferUploadRequest` 并调 `Queue::UploadBuffer`，SHALL NOT 内含 staging 逻辑、`isUMA` 分支或 in-flight 判定。

#### Scenario: resource 层无 staging 逻辑

- **WHEN** 阅读 `aurora/resource` 的上传实现
- **THEN** 无 staging buffer 分配、无 `isUMA` 判断、无 in-flight 判定；上传意图全部经 `Queue::UploadBuffer` 委托 RHI

