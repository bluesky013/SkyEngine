## Context

Aurora 的 RHI 接口层（`aurora/rhi/interface`）只有裸资源：`rhi::Buffer`（`Descriptor{size, usage, memory}` + `Map/UnMap`）与 `rhi::Image`（`Descriptor{...}`），二者都继承 `RefObject` + `IDelayReleaseResource`，由 `Device::CreateBuffer/CreateImage` 创建。RHI 层**没有**统一的资源基类，也没有「惰性创建 / 上传」这些渲染层常用语义。

渲染层（`GlobalRenderResources`、`PipelinePass`、RDG batch tier）目前要么直接操作裸 `rhi::Buffer`，要么还没落地。本 change 在 `aurora/core` 内新增 `aurora/resource` 子目录，提供一个**资源封装层**：统一 `RenderResource` 基类（buffer 与 image 的公共基类），提供惰性创建、统一上传两件事（资源/显存统计归后端，见 D7）；并在此基础上落地 buffer 侧的 tier 模板与带解释元数据的具体类型。上传能力由独立的 `aurora-upload` change 提供，本 change 只声明上传意图并委托。

关键认知：

1. `RenderResource` **不是** buffer 子区间视图（不引入/复用 `rhi::BufferView`），而是「对 rhi 资源的一次封装」——buffer 与 image 都是它的子类。
2. buffer 资源有两个正交维度：**存储/更新策略**（static / dynamic / transient）与**解释**（vertex / index / uniform / storage）。vertex/index 携带真实解释元数据（语义、stride、index type），storage 是通用、可重解释的视图。

## Goals / Non-Goals

**Goals:**

- 统一 `RenderResource` 基类，作为 buffer 与 image 的公共基类，提供：**惰性创建**（实际需要时才 `CreateBuffer/CreateImage`）、**统一上传**（`Upload` 委托 RHI 上传能力）。
- 三档存储 tier 模板：`StaticBuffer`（GPU_ONLY，`Upload` 委托 `Queue::UploadBuffer`）、`DynamicBuffer`（CPU_TO_GPU，CPU 上传 + 同步 inflight frame）、`TransientBuffer`（每帧内使用，`TransientBufferPool` 每帧分配）。
- 带解释元数据的 `VertexBuffer`（语义 + stride + input rate）、`IndexBuffer`（IndexType）、`UniformBuffer`（UBO 视图）。
- 通用 `StorageBuffer`：usage 超集（STORAGE | VERTEX | INDEX | INDIRECT），可被重解释为 vertex/index/indirect，覆盖「CS 输出 → 后续消费」链路。
- 最小测试：惰性创建、上传、tier 语义、解释元数据、StorageBuffer 重解释。

**Non-Goals:**

- 本 change 只做 **buffer** 侧具体类型；image 子类（Texture/RenderTarget）只预留基类位置，不实现。
- 不修改 `aurora/rhi` 接口；上传能力由独立的 `aurora-upload` change 提供。
- 不接入 RDG `BatchAllocator` / `TransientPool`（后续 change 再统一）。
- 不引入新的 CMake 模块；资源层落在现有 `Aurora`（core）目标内。

## Decisions

### D1: 位置与模块边界

新代码放在 `engine/aurora/core/include/aurora/resource/` 与 `engine/aurora/core/src/resource/`，纳入现有 `Aurora`（core）库的 `file(GLOB_RECURSE ...)` 自动收集，不改 `CMakeLists.txt`。

- **理由**：与 `aurora/core/include/aurora/scene/` 目录结构一致；避免为几个头文件新建独立模块。
- **备选**：新建 `aurora/resource` 独立模块 —— 被否，当前规模过小，等 resource 层变大再拆。

### D2: `RenderResource` 是 buffer/image 的公共基类（资源封装，非视图）

```cpp
namespace sky::aurora {
class RenderResource {
public:
    virtual ~RenderResource();

    const Name&  GetName() const;
    rhi::Device* GetDevice() const;
    bool         IsCreated() const;

    // 统一上传：把 host 数据上传到 device（staging 策略由 RHI 决定）
    virtual bool Upload(const void* data, uint64_t size, uint64_t offset = 0) = 0;

protected:
    explicit RenderResource(const Name& name);

    rhi::Device* device_  = nullptr;
#if SKY_ENABLE_RESOURCE_NAME
    Name         name_;          // 仅在 resource-name 模式引入（见 aurora-resource-name）
#endif
    bool         created_ = false;

    virtual void Create()  = 0;   // 惰性：首次需要时才创建底层 rhi 资源；创建时把 name_ 写入 Descriptor::name
    virtual void Release() = 0;
};
}
```

- **理由**：`RenderResource` 的核心职责是对 rhi 资源（buffer 或 image）的**封装**，而非描述「某段内存如何被解读」。因此它不持有 `offset/range`，不引入 `rhi::BufferView`。它提供渲染层最需要、而裸 rhi 没有的两件事：
  1. **惰性创建**：构造时只存 `device` + descriptor，`Create()` 在首次访问时才真正 `device->CreateBuffer/CreateImage`。
  2. **统一上传**：`Upload(data, size, offset)` 声明上传意图，具体 staging/直接写策略由 RHI 的 `Queue::UploadBuffer` 决定（见 D9）。
- 基类非模板，便于 `std::vector<RenderResource*>` 统一持有 buffer 与 image 资源。
- **name 受宏守卫**：`name_` 仅在 `SKY_ENABLE_RESOURCE_NAME == 1` 下存在（见 `aurora-resource-name` change）；`Create()` 时把 `name_` 写入 `Descriptor::name` 下传后端 debug label；宏关闭时无 name 成员、零开销。
- **备选**：`RenderResource` 直接持有 `rhi::BufferPtr + offset + range`（buffer 视图）—— 被否，会把基类绑死到 buffer 且与 image 冲突；`RenderResource` 只做纯 marker 无任何职责 —— 被否，失去了上传/惰性创建的统一收口点。

### D3: buffer kind tag（仅供 tier 声明 usage，不含语义）

```cpp
struct VertexBufferKind   { static constexpr rhi::BufferUsageFlags Usage = rhi::BufferUsageFlagBit::VERTEX; };
struct IndexBufferKind    { static constexpr rhi::BufferUsageFlags Usage = rhi::BufferUsageFlagBit::INDEX; };
struct UniformBufferKind  { static constexpr rhi::BufferUsageFlags Usage = rhi::BufferUsageFlagBit::UNIFORM; };
struct StorageBufferKind  { static constexpr rhi::BufferUsageFlags Usage =
                              rhi::BufferUsageFlagBit::STORAGE | rhi::BufferUsageFlagBit::VERTEX
                            | rhi::BufferUsageFlagBit::INDEX   | rhi::BufferUsageFlagBit::INDIRECT; };
```

- **理由**：kind tag 只负责告诉 tier 模板创建底层 buffer 用哪些 usage bit；vertex 的 stride/semantic、index 的 index type 由具体类型携带。`StorageBufferKind` 用超集 usage，使其可重绑定为 vertex/index/indirect。
- **备选**：把语义/stride/index type 也塞进 tag —— 被否，会让 tag 变成完整资源，无法与 tier 正交组合（见 D6）。

### D4: tier 模板接口（buffer 侧，kind 作为模板参数，继承 RenderResource）

```cpp
template <typename Kind> class StaticBuffer    : public RenderResource { ... };
template <typename Kind> class DynamicBuffer   : public RenderResource { ... };
template <typename Kind> class TransientBuffer : public RenderResource { ... };
```

- **理由**：tier 决定内存与更新策略，与「用途/解释」正交；`StaticBuffer<VertexBufferKind>` 与 `StaticBuffer<UniformBufferKind>` 共享实现、仅 `Kind::Usage` 不同。纯模板无虚表、无 RTTI。
- **备选**：单 `Buffer` 类 + tier enum + 运行时分支 —— 被否，失去编译期 type-safety 与零开销分派。

### D5: 各 tier 的内存与上传方式

| tier | `MemoryType` | 上传方式 |
|---|---|---|
| `StaticBuffer` | `GPU_ONLY` | **委托 RHI 上传**：`Upload` 构造 `BufferUploadRequest` 并调 `Queue::UploadBuffer`（staging vs 直接写由 `aurora-upload` 决定），一次性，之后不可写；`Map()` 返回 `nullptr` |
| `DynamicBuffer` | `CPU_TO_GPU` | **CPU 上传**：持久映射，`Map()`/`Write(data, size, offset)` 直接写；**需同步 inflight frame**（ring/double-buffer 或 fence 防止覆盖在途帧数据） |
| `TransientBuffer` | `CPU_TO_GPU`（按池策略） | **`TransientBufferPool` 每帧分配**：帧内从 `TransientBufferPool` 分配子区间，仅当前帧有效 |

- **理由**：三种上传路径对应三种资源生命周期——static 只写一次、委托 RHI 上传（staging vs 直接写由 `aurora-upload` 决定）；dynamic 每帧 CPU 写、需与在途帧同步；transient 帧内临时、由每帧分配器兜底。惰性创建把「分配底层 rhi 资源」推迟到真正需要时（`RenderResource::Create()` 的 buffer 特化）。
- **备选**：全部走 `CPU_TO_GPU` 动态 —— 被否，GPU_ONLY 在独立显存 GPU 上带宽更优；static 走持久映射 —— 被否，浪费 host 可见内存且无法享受 device-local 带宽。

### D6: 具体 buffer 类型 = storage tier 模板 + 解释元数据

具体类型是**模板化于 storage tier、带默认 tier** 的类，携带各自解释元数据：

```cpp
// 顶点布局：一个 binding 的 stride + 语义 + 输入速率（对齐 VertexBindingDesc/VertexAttributeDesc）
struct VertexLayout {
    uint32_t            stride    = 0;
    rhi::VertexInputRate inputRate = rhi::VertexInputRate::PER_VERTEX;
    VertexSemanticMask  semantics;          // 该 buffer 提供哪些语义
};

template <typename Storage = StaticBuffer<VertexBufferKind>>
class VertexBuffer : public Storage {
    VertexLayout layout_;
public:
    const VertexLayout& GetLayout() const;
};

template <typename Storage = StaticBuffer<IndexBufferKind>>
class IndexBuffer : public Storage {
    rhi::IndexType indexType_ = rhi::IndexType::NONE;
public:
    rhi::IndexType GetIndexType() const;
};

template <typename Storage = DynamicBuffer<UniformBufferKind>>
class UniformBuffer : public Storage { };

template <typename Storage = DynamicBuffer<StorageBufferKind>>
class StorageBuffer : public Storage {
public:
    // 通用重解释：同一块底层 buffer 的不同子区间产出不同用途视图
    rhi::Buffer* AsVertex(uint64_t offset) const;
    rhi::Buffer* AsIndex(uint64_t offset) const;
    rhi::Buffer* AsIndirect(uint64_t offset) const;
};
```

- **理由**：
  - `VertexBuffer` 携带 `VertexLayout`（stride + 语义 mask + input rate），因为顶点缓冲的消费方需要知道数据布局，而非只一个 `VERTEX` usage bit。
  - `IndexBuffer` 携带 `IndexType`，因为 `BindIndexBuffer(buffer, offset, type)` 需要它。
  - `StorageBuffer` 是**通用视图**：usage 超集 + `AsVertex/AsIndex/AsIndirect` 重解释，使其既能当 SSBO，也能作为 CS 输出后继续被读为 vertex/index/indirect。重解释只暴露底层 `rhi::Buffer*`（绑定偏移由调用方在 `Bind*` 时给），不引入 `rhi::BufferView`。
  - 类型模板化于 storage tier，`VertexBuffer<>` 默认 static，`VertexBuffer<DynamicBuffer<VertexBufferKind>>` 即动态顶点缓冲，`kind × tier` 全组合可表达且不重复元数据定义。
- **备选**：VertexBuffer 固定继承一个 tier（非模板）—— 被否，失去动态/瞬态顶点缓冲；把 layout 塞进 kind tag —— 被否，破坏「存储 vs 解释」正交性；用 `rhi::BufferView` 表达重解释 —— 被否，见 D2。

### D7: 统计归后端（VMA / D3D12MA），resource 层不设 RenderResourceManager

「实际创建了多少 buffer/image、按类别占用了多少显存」只有后端知道——aurora 的 Vulkan 后端用 VMA（`VmaAllocator`）、DX12 后端用 D3D12MA（`D3D12MA::Allocator`）做显存池，分配统计（按 heap / memory type 的 budget 与 usage）由这些 allocator 提供。

因此：

- resource 层 SHALL NOT 自建 `RenderResourceManager`（登记表 + 计数 + 内存统计）。`RenderResource` 只是对 rhi 资源的逻辑封装，无法得知真实显存占用（VMA/D3D12MA 有子分配、对齐、pool 等内部结构）。
- 资源/显存统计由 **RHI 层**提供——未来在 `Device` 上暴露 `GetMemoryStats()` 之类的接口，读 VMA/D3D12MA 统计。本 change 不实现，`RenderResource` 仅概念上「可被后端统计管理」。

- **理由**：显存池在后端（VMA/D3D12MA），物理分配/回收都发生在那里；resource 层重复计数既不准又重复。
- **备选**：resource 层自建统计 —— 被否，无法反映真实显存，且与后端 allocator 的权威统计重复。

### D8: TransientBufferPool 与 RDG TransientPool 严格区分

`TransientBufferPool` 作为**非模板**类放 `src/resource/`（`TransientBufferPool.{h,cpp}`），`TransientBuffer<Kind>` 持有其指针。**每帧分配**：帧内 `Allocate(desc) -> 子区间`（对齐 `minUniformBufferOffsetAlignment`），`Reset()` 在帧末回收。

**与 RDG `TransientPool` 的关系**：二者是**不同层、不同语义**，不得混用：

| | `TransientBuffer`（本 change，resource 层） | RDG `TransientPool`（rhi/interface/src/rdg/） |
|---|---|---|
| 定位 | 每帧内使用的 buffer 类型 | RDG 每帧可复用的资源池（Image + Buffer 整对象 Acquire/Release） |
| 生命周期 | 仅当前帧有效 | 跨帧复用（按完整 descriptor 键控） |
| 用途 | 渲染层每帧 scratch / 上传 | RDG 内 transient 资源分配 |
| 实现 | `TransientBufferPool`（resource 层每帧分配器） | `ObjectPool`/`TransientPool`（RDG 内部） |

- **理由**：`TransientBuffer` 是「每帧内使用的 buffer」这一资源语义，`TransientPool` 是 RDG 的「跨帧复用机制」，命名同源但职责完全不同，必须显式区分以免误导。
- **备选**：复用 RDG `TransientPool` 做 TransientBuffer 的分配 —— 被否，RDG 池是 RDG 内部机制，与 pass 生命周期耦合，不应被资源层直接依赖。

### D9: 上传委托 RHI（aurora-upload change）

staging 上传**不是** resource 层的职责。本 change 的 `RenderResource::Upload` / `StaticBuffer::Upload` 只构造 `BufferUploadRequest`（`IUploadStream` 源）并调 `Queue::UploadBuffer`，不含任何 staging 逻辑。

上传路径（staging vs 直接写、UMA vs discrete、以及 **in-flight 判定**）由独立的 `aurora-upload` change 在 RHI 层决定——它引入 `DeviceCapability::isUMA` + `Queue::UploadBuffer/Image` + per-frame staging ring，并按「内存拓扑 × 在途状态」选择路径。要点：**即使 UMA，目标在途（GPU 正在读）时也不能直接写**，需 staging 或 fence 等待。

- **理由**：resource 层只关心「把这段数据放到这个资源」，不关心内存拓扑与同步细节；把 UMA/discrete/in-flight 的分叉留在 RHI，resource 层零 staging 代码。
- **备选**：resource 层自行判 `isUMA` 分叉 —— 被否，拓扑知识泄漏到 resource 层；resource 层硬编码 staging —— 被否，UMA 非在途时白多一次 copy，且仍遗漏 in-flight 竞争。

## Risks / Trade-offs

- **[Static 上传依赖 aurora-upload]** `StaticBuffer::Upload` 依赖 `Queue::UploadBuffer`（由 `aurora-upload` change 提供）→ 缓解：本 change 在 proposal/Impact 显式声明该依赖；`aurora-upload` 未落地前 `StaticBuffer::Upload` 标 TODO。
- **[惰性创建与线程安全]** 首次访问才创建，可能发生在渲染线程 → 缓解：v1 约定「创建在提交前单线程完成」，多线程延后到后续 change。
- **[StorageBuffer 重解释与 barrier 责任边界]** CS 写 storage 再当 vertex/index 读，需调用方自行插 barrier → 缓解：本层只暴露重解释，不隐含 barrier；职责归属写入 spec。
- **[TransientBufferPool 与 RDG TransientPool 命名相近]** 二者职责不同但命名易混 → 缓解：D8 显式列表区分；`TransientBufferPool` 仅做资源层每帧分配，预留「外部 allocator 注入」钩子（后续如需与 RDG 池统一再替换底层实现）。
- **[模板头文件膨胀]** 全 header-only 模板 → 缓解：kind/tier 模板保持极薄，非模板逻辑下沉 `src/resource/`。
- **[默认 tier 语义易混淆]** `VertexBuffer<>` 默认 static → 缓解：文档明确默认 tier，动态变体显式写 `VertexBuffer<DynamicBuffer<VertexBufferKind>>`。

## Migration Plan

- 纯新增代码，无现有调用方，无迁移。
- 回滚：删除 `include/aurora/resource/`、`src/resource/` 与对应测试即可。

## Open Questions

- image 子类（Texture/RenderTarget）何时落地——本 change 仅预留基类，不实现。
