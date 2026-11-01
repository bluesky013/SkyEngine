# aurora-resources Specification

## Purpose

Define mesh, material, skinning and renderer-resource data and ownership.

## Scope notes

### aurora-resource

TBD - consolidated from: aurora-render-buffers aurora-render-geometry aurora-render-textures aurora-upload

### aurora-skinning

Define immutable inverse-bind Skin resources, geometry-only Mesh data and instance-selected Skin/Skeleton combinations. Runtime rig types remain in Animation; Aurora adaptor owns the Skeleton asset bridge, while Aurora core stays independent of Animation.

## Requirements

### Requirement: Material 内部按 technique 维护

Material SHALL own tagged MaterialTechnique records containing an explicit ShaderTechnique program (stage/source/entry), checked legacy aliases and material render state. Scene queues SHALL select matching tags; fullscreen and compute SHALL select programs explicitly without requiring Material. Empty or duplicate Material technique tags SHALL fail. MaterialRenderState SHALL contain depth/stencil, raster, blend and alpha-to-coverage state, excluding mesh vertex layout/topology and Pass sample count. Shader resolution SHALL retain the selected program rather than substitute an implicit built-in shader.

#### Scenario: Tagged stage programs
- **WHEN** opaque and shadow techniques reference different explicit stage programs
- **THEN** each queue uses its matching program and material state

#### Scenario: Invalid tag
- **WHEN** a technique has an empty or duplicate tag
- **THEN** adding it fails without changing the existing techniques

### Requirement: Material 属性系统（material 级，值 + 纹理）

Material SHALL 提供跨 technique 共享的 Name 属性，属性类型继续复用 RgFieldType 的词汇，不建立另一套材质字段类型枚举。CPU 值按其声明语义存储，字节大小由类型派生；MaterialPropertyMap SHALL 只描述 CPU 属性位置，不表示 GPU offset。SetValue/GetValue SHALL 校验 T 的语义类型/shape，SHALL NOT 仅凭 sizeof 通过校验；未知名或不匹配 SHALL 返回明确结果且不写入。

纹理 SHALL 按 Name 持有合法的 Texture 引用/逻辑资源身份，sampler SHALL 使用独立的类型化设置/引用，不能作为 Texture 伪装。新资产的参数 Key SHALL 来自完整 Material group 声明、唯一 cbuffer 字段与 Shader-local 选项，按 canonical Name 映射；显式 aliases SHALL 仅作为旧数据的受检迁移输入，最终进入ResourceGroup 共用的同一个 RHI 命名事务前端及布局计划，不暴露平台 slot/offset。CPU 属性可保留既有类型词汇，但执行路径不支持的字段/资源 SHALL 在受检绑定前明确 Unsupported；本次不自动扩大 shader 字段支持集合。

#### Scenario: 按类型声明并读写
- **WHEN** 声明 FLOAT4 的 baseColor 并按 Name 写入 Vector4
- **THEN** 可读回对应 CPU 值，GetPropertyMap 保留该 CPU 属性的类型与位置

#### Scenario: 相同大小不能替代类型
- **WHEN** 对 FLOAT 属性按 Name 写入 uint32_t
- **THEN** 明确类型错误且原值不变，即使二者同为四字节

#### Scenario: Size 由 type 派生
- **WHEN** 读取 material CPU 属性元数据
- **THEN** 值大小由声明语义类型派生，不重复存储另一份可漂移 size，也不当作 GPU block offset

#### Scenario: 类型词汇共用
- **WHEN** material 描述已有 scalar/vector/matrix 或资源属性
- **THEN** 复用共享字段词汇，执行能力由实际支持表校验

#### Scenario: 纹理与 sampler 挂接
- **WHEN** 按 Name 分别设置合法纹理与 sampler
- **THEN** 各自进入匹配逻辑资源，类型不兼容时明确失败，sampler 不以 Texture 容器表示

### Requirement: MaterialInstance 部分字段 runtime 稀疏覆盖

MaterialInstance SHALL 持有共享 Material 与实例覆盖。值覆盖 SHALL 延续单个连续 CPU 存储、首次写入惰性复制的表示，不逐字段分配容器；纹理/资源覆盖 SHALL 保持稀疏表示。Name 查找与语义类型校验 SHALL 与 Material 相同，失败返回明确结果且保留旧值。未覆盖项 SHALL 继承 Material 默认值，实例更新 SHALL 不污染共享 Material。

SetMaterial SHALL 清空该实例旧覆盖；IsOverridden SHALL 表达显式覆盖是否存在，包括显式写入与默认值相同的值。未设置 Material 的写入 SHALL 返回明确错误而非静默成功，读取返回缺失。实例最终参数 SHALL 按 technique/interface 的 canonical Name 进入与 ResourceGroup 共用的 RHI 更新事务前端，并冻结对应值/资源版本供帧使用。

#### Scenario: 部分覆盖与继承
- **WHEN** 实例只按 Name 覆盖 baseColor
- **THEN** baseColor 使用实例值，其它属性继承默认值，覆盖状态可查询

#### Scenario: 覆盖不污染共享 Material
- **WHEN** 实例更新值、纹理或 sampler
- **THEN** 共享 Material 及其它实例保持原值

#### Scenario: 值覆盖保持连续存储
- **WHEN** 一个实例只覆盖一个 scalar 字段
- **THEN** 值仍存于实例的单块连续存储，不逐字段创建独立容器

#### Scenario: 未设置 Material
- **WHEN** 未关联 Material 的实例执行写入或读取
- **THEN** 写入返回明确错误，读取返回缺失，不崩溃也不伪装成功

#### Scenario: 更新与在途帧隔离
- **WHEN** 旧帧未退休时实例替换贴图或改变数值
- **THEN** 新帧使用新快照，旧帧继续读取旧值与精确图片版本

### Requirement: Generated Material parameter keys

Material logical keys SHALL be generated from the stable Group 2 declaration union of its techniques and Shader-local option schemas. Keys SHALL distinguish fields, textures, samplers and options, retain semantic types and avoid persisting target byte offsets as authoring schema. Cross-technique shared keys SHALL agree on semantics/type/shape. Display names SHALL NOT create implicit aliases. Existing aliases SHALL migrate values explicitly and incompatible mappings SHALL fail diagnostically.

#### Scenario: Technique subset
- **WHEN** one technique uses only part of the Material's logical key table
- **THEN** other known keys remain stored without requiring active bindings for that technique

### Requirement: One logical Material cbuffer

A linked program family SHALL declare zero or one logical Group 2 cbuffer after merging stages. Repeated references to the same block SHALL count once; distinct blocks SHALL fail even if only one is active in a particular choice. Target stage ABI for the shared block SHALL be compatible. Pass/Batch groups SHALL retain their own resource contracts.

#### Scenario: Two material blocks
- **WHEN** Group 2 declares two distinct buffers
- **THEN** schema validation rejects the program before publication

### Requirement: Inactive Material values remain valid

Known inactive keys SHALL retain values and skip only current target writes. Unknown keys, semantic type errors and missing required active values/resources SHALL fail. Required resources SHALL be determined by the actual target contract, not inferred from a false option. Texture residency SHALL NOT implicitly change static options. MaterialInstance SHALL keep value/texture/sampler overrides without new static-option overrides.

#### Scenario: Disabled texture option
- **WHEN** a texture option is false but the native artifact still requires the texture
- **THEN** binding supplies a valid resource or explicit default, or reports missing-resource

#### Scenario: Inactive wrong type
- **WHEN** an inactive known field receives the wrong semantic type
- **THEN** validation still rejects it

### Requirement: CPU 侧 mesh 数据接口

`MeshVertexDataInterface` SHALL 是类型无关的顶点数据抽象，提供 `Resize(uint32_t)` / `GetStride()` / `Count()` / `GetData()`（含 const 重载），以及模板 `SetVertexData<T>(index, value, offset)` / `GetVertexData<T>(index, offset)`。`TRawMeshVertexData<T>` SHALL 是其具体实现，`GetStride() == sizeof(T)`，数据用 `core/archive/BinaryData`（`BinaryDataPtr`）承载。

`RawMeshIndexData` SHALL 按 `IndexType`（U16/U32）存取值，提供 `Resize` / `Count` / `GetData` / `SetIndex` / `GetIndex` / `GetIndexType`，数据用 `BinaryData` 承载。

#### Scenario: 顶点数据构建与存取

- **WHEN** 用 `TRawMeshVertexData<Vertex>` 建 N 个顶点，`Resize(N)` 后逐顶点 `SetVertexData`
- **THEN** `Count() == N`、`GetStride() == sizeof(Vertex)`；`GetVertexData(i)` 读回与写入一致

#### Scenario: 索引数据 U16/U32

- **WHEN** 用 `RawMeshIndexData(type=U32)` `SetIndex(i, v)` 后 `GetIndex(i)`
- **THEN** 读回 `v`；`GetData()` 返回可喂 `IndexBuffer::Upload` 的字节指针，`GetIndexType() == U32`

### Requirement: Mesh 是 RenderGeometry 之上的纯封装

`Mesh` SHALL 继承 `RefObject`，持有 `CounterPtr<RenderGeometry>` + `std::vector<SubMesh>` + `std::vector<BlendShape>`，提供 `SetGeometry` / `GetGeometry` / `AddSubMesh` / `GetSubMeshes` / `AddBlendShape` / `GetBlendShapes` / `GetBlendShapeCount` / `GetName` / `GetLocalBounds`（委托 `geometry->GetLocalBounds()`，返回 `BoundingBoxSphere`）。`Mesh` SHALL NOT 直接持有底层 buffer（buffer 在 `RenderGeometry` 里）。

`SubMesh` SHALL 含 `firstVertex` / `vertexCount` / `firstIndex` / `indexCount` / `materialIndex` / `bounds`（`BoundingBoxSphere`）。`firstVertex`/`vertexCount` 定义子网格顶点区间，`firstIndex`/`indexCount` 定义索引区间。`materialIndex` 是材质/technique 占位索引。

#### Scenario: 挂接 geometry 与 submesh

- **WHEN** `mesh.SetGeometry(geo)` 后 `mesh.AddSubMesh({firstVertex=0, vertexCount=24, firstIndex=0, indexCount=36, materialIndex=0, bounds})`
- **THEN** `GetGeometry() == geo`，`GetSubMeshes().size() == 1` 且字段一致；`GetName()` 返回构造时传入的 Name

### Requirement: BlendShape 是 delta 增量数据

`BlendShape` SHALL 含 `name` + `positionDeltas`（`std::vector<Vector3>`）+ 可选 `normalDeltas` / `tangentDeltas`（`std::vector<Vector3>`），与 base 顶点一一对应。语义 SHALL 为 `final = base + Σ weightᵢ·deltaᵢ`（对齐 Unity / glTF morph target）。

本能力 SHALL 只落地 delta 数据结构；GPU morphing（morph buffer + shader 混合）SHALL NOT 在本能力内实现。

#### Scenario: 注册 blend shape

- **WHEN** `mesh.AddBlendShape({name, positionDeltas=N 个, normalDeltas=N 个, tangentDeltas 空})`
- **THEN** `GetBlendShapeCount() == 1`；`GetBlendShapes()[0].positionDeltas.size() == N`

#### Scenario: GPU 混合不在本能力

- **WHEN** 阅读 `aurora/resource/Mesh.h`
- **THEN** 无 morph buffer、无 shader 混合代码（blend shape 仅数据结构）

### Requirement: 蒙皮输入槽与独立实例绑定

VertexSemantic SHALL 保留 JOINTS / WEIGHTS 及名称解析。JOINTS SHALL 表示四个 Skin 输入槽索引，WEIGHTS SHALL 表示对应权重，数据留在 RenderGeometry 顶点流。Mesh SHALL 不定义或引用 Aurora Skeleton，不持有选定 Skin，不暴露 SetSkeleton/GetSkeleton/HasSkin 或 SetSkin/GetSkin。Skin 与实例绑定、palette 和 GPU 执行 SHALL 遵守 aurora-skinning capability；本类型不直接管理 GPU 蒙皮任务。

#### Scenario: 蒙皮顶点语义
- **WHEN** 读取含 JOINTS / WEIGHTS 的顶点布局
- **THEN** 语义名称 SHALL 能解析，JOINTS SHALL 按绑定槽解释而非直接索引 Skeleton

#### Scenario: 共享 mesh 的实例绑定隔离
- **WHEN** 两个实例对同一 Mesh 选择不同 Skin
- **THEN** 几何资源 SHALL 保持不变，所选绑定 SHALL 由各实例保存

### Requirement: RenderResource 是 buffer/image 的公共资源封装基类

`RenderResource` SHALL 继承 `RefObject`，作为 buffer 与 image 资源的统一非模板基类，作用是对 rhi 资源的一次封装，提供：

- **引用计数**：SHALL 继承 `RefObject`，使 `CounterPtr<Texture>` / `CounterPtr<VertexBuffer>` 等可共享资源；`unique_ptr` 拥有语义（如 `RenderGeometry` 持有 `VertexBuffer`）SHALL 保持可用。
- **惰性创建**：构造时仅持有 `rhi::Device*` 与（`SKY_ENABLE_RESOURCE_NAME == 1` 时的）`Name`，底层 `rhi::Buffer`/`rhi::Image` SHALL 在首次访问（`Create()`）时才创建；`IsCreated()` SHALL 反映是否已创建。
- **统一上传**：`Upload(const void* data, uint64_t size, uint64_t offset = 0)` SHALL 把 host 数据上传到 device 资源，上传策略（staging vs 直接写）SHALL 委托 RHI（见 `aurora-upload` spec），resource 层 SHALL NOT 内含 staging 逻辑。

`RenderResource` SHALL NOT 引入或持有 `rhi::BufferView`，也 SHALL NOT 持有 `offset/range` 视图字段。`name_` SHALL 仅在 `SKY_ENABLE_RESOURCE_NAME == 1` 时存在（见 `aurora-resource-name` spec），`Create()` 时 SHALL 把名字下传 `Descriptor::name`。

#### Scenario: 惰性创建

- **WHEN** 构造一个 buffer 资源但尚未访问其底层 buffer
- **THEN** `IsCreated() == false`，底层 `rhi::Buffer` 尚未创建；首次 `Upload`/访问后 `IsCreated() == true`

#### Scenario: 统一上传入口

- **WHEN** 对一个 buffer 资源调用 `Upload(data, size, offset)`
- **THEN** host 数据被上传到 device 资源指定偏移处，且该资源完成惰性创建

#### Scenario: 引用计数共享

- **WHEN** 两个 `CounterPtr<Texture>` 指向同一 `Texture`，其中一份释放
- **THEN** `Texture` 仍存活；最后一个引用释放时 `Texture` 析构（析构前同步 pending 上传，见 `aurora-upload`）

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

`RenderGeometry` SHALL 是一个 `RefObject` 复合资源（**不**继承 `RenderResource`），持有：顶点流 `std::vector<std::unique_ptr<VertexBuffer<>>>`（每个自带 `VertexLayout`）、可选索引缓冲 `std::unique_ptr<IndexBuffer<>>`（自带 `IndexType`）、本地 `BoundingBoxSphere localBounds`。

`RenderGeometry` SHALL 提供 `AddVertexStream(std::unique_ptr<VertexBuffer<>>)` / `SetIndexBuffer(std::unique_ptr<IndexBuffer<>>)` 转移所有权，以及 `GetVertexStreams()` / `GetIndexBuffer()` / `GetLocalBounds()` / `GetName()` 访问器。

`RenderGeometry` SHALL 是**纯封装**：SHALL NOT 持有 host 数据、SHALL NOT 提供 `Upload()`（上传是调用方 + buffer 自身职责）。

#### Scenario: 组合顶点流 + 索引 + 包围盒

- **WHEN** 创建一个 `RenderGeometry`，`AddVertexStream` 一个 `VertexBuffer`、`SetIndexBuffer` 一个 `IndexBuffer`、`SetLocalBounds` 一个 `BoundingBoxSphere`
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

### Requirement: 包围盒为本地空间 BoundingBoxSphere

`RenderGeometry` 的 `localBounds` SHALL 是模型/本地空间的 `BoundingBoxSphere`（center + extent + radius）；世界空间包围盒由 scene 的 `WorldInfo` 变换推导，SHALL NOT 在 geometry 内重复存储。

#### Scenario: 本地包围盒可查询

- **WHEN** 设置 `localBounds` 为某 `BoundingBoxSphere` 后调用 `GetLocalBounds()`
- **THEN** 返回值与该 `BoundingBoxSphere` 一致

### Requirement: aurora 不定义 Skeleton

Aurora SHALL NOT 定义 `Skeleton` / `Bone` 类型，SHALL NOT 新增 `aurora/animation` 目录或模块。动画 rig 属于独立模块 `engine/animation`（target `Animation`，仅依赖 `Core`）。`aurora/core` SHALL NOT 依赖 `Animation`。

#### Scenario: 无 aurora Skeleton

- **WHEN** 检索 `sky::aurora` 命名空间
- **THEN** 不存在 `Skeleton` / `Bone` 类型，也不存在 `aurora/animation/` 头文件

### Requirement: Skin 自持 mesh 侧蒙皮绑定

`Skin`（`aurora/resource/Skin.h`）SHALL 为独立、发布后不可变的绑定资源，包含有序输入槽契约、每槽 inverseBindMatrices 和目标骨的规范名称/路径或索引。它 SHALL 由 Aurora 导入/cook 生成独立 AuroraSkin 产品，SHALL NOT 内嵌在 Mesh bulk、包含必需 Skeleton UUID 或持有求值后 boneMatrices。slot -> joint 映射 SHALL 在组合 Skin 与选定骨架时解析并验证。Skin SHALL NOT 引用动画模块类型；palette SHALL 由实例/帧持有。

#### Scenario: 设置绑定数据
- **WHEN** 构建 Skin 的 inverseBindMatrices 与目标骨标识后查询
- **THEN** getter SHALL 返回对应数据，发布后的绑定 SHALL 不被实例播放修改

#### Scenario: 不依赖动画类型
- **WHEN** 编译 aurora/core
- **THEN** SHALL 不需要 Animation 模块，Skin 头文件 SHALL 不含动画类型

#### Scenario: 两实例共享 mesh
- **WHEN** 两个 renderer entity 使用同一 Skin 并播放不同姿势
- **THEN** 两者 SHALL 持有独立 palette 和 Pose revision，共享绑定 SHALL 保持不变

#### Scenario: 不同 mesh 共用 skeleton
- **WHEN** 两个实例选择同一 Skeleton 而不同 Mesh 需要不同 inverse bind matrices
- **THEN** 实例 SHALL 分别选择自己的 Skin，不因 Skeleton UUID 相同而复用错误绑定

#### Scenario: 同一 mesh 使用不同绑定
- **WHEN** 两个实例引用同一 Mesh，分别选择 Skin A / Skeleton A 与 Skin B / Skeleton B，且两组绑定通过验证
- **THEN** 两者 SHALL 共享几何并生成各自 palette，切换一方的 Skin SHALL NOT 修改 Mesh 或另一方的绑定

### Requirement: 场景组件为 SkinnedMesh

`aurora/scene/SceneTypes.h` SHALL 保留场景组件 `SkinnedMesh` 与 tag `sky.aurora.SkinnedMesh`，关联不可变 Skin 及实例姿势/调色板状态。它 SHALL 不与 Framework Actor 的 SkeletalMeshComponent 混用，也 SHALL 不使用 Skin 作为场景组件名。销毁后的旧 generation SHALL 被拒绝。

#### Scenario: 注册 SkinnedMesh 组件
- **WHEN** 对实体 Add<SkinnedMesh> 后查询并销毁该实体
- **THEN** 有效代际的 Get SHALL 返回数据，销毁后旧代际 SHALL 被拒绝且在途帧数据仍受 lease 保活

### Requirement: Mesh 与实例 Skin 绑定分离

Mesh SHALL 仅保存几何与 influence 顶点数据及其输入槽契约，SHALL NOT 持有 Skin / Skeleton 资源引用或暴露 SetSkin/GetSkin/HasSkin、SetSkeleton/GetSkeleton/HasSkeleton。是否具有 influence 数据 SHALL 由明确顶点布局能力表达。绑定 SHALL 由场景实例选择；导入默认组合 SHALL 保存在导入元数据并复制为组件的显式引用。

#### Scenario: 实例切换皮肤
- **WHEN** 一个实例从 Skin A 切换为 Skin B
- **THEN** 共享 Mesh 内容 SHALL 不变，实例 SHALL 重新验证完整组合后再提交新绑定

#### Scenario: 仅创建几何资源
- **WHEN** 创建带 JOINTS / WEIGHTS 的 Mesh 而未创建场景实例
- **THEN** Mesh SHALL 不隐式分配 Skin、选择 Skeleton 或创建 palette
