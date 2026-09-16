## Context

`aurora-render-buffers` 已落地 `RenderResource` 基类 + buffer 侧三层结构（`engine/aurora/core/include/aurora/resource/Buffer.h`）：

- 4 个 kind tag（`VertexBufferKind`/`IndexBufferKind`/`UniformBufferKind`/`StorageBufferKind`），各自静态声明 `BufferUsageFlags Usage`（Buffer.h:48-60）。
- 3 个 tier 模板（`StaticBuffer<Kind>`/`DynamicBuffer<Kind>`/`TransientBuffer<Kind>`），`Create()` 里用 `Kind::Usage` 建底层 buffer（Buffer.h:115 / 210 / 283）。
- 4 个具体类型（`VertexBuffer<Storage>`/`IndexBuffer<Storage>`/`UniformBuffer<Storage>`/`StorageBuffer<Storage>`），模板化于 Storage tier（Buffer.h:310/323/336/346）。

设计初衷是表达 `tier × kind` 全组合。但实际使用只有默认组合，非默认组合与显式 `Kind` 实例化均为 **0 处**；`Kind` 的 `Usage` 只被上述 3 处 `Create()` 消费。`TransientBuffer` 0 使用、0 测试（stub，`TransientBufferPool` 是 follow-up）。

`aurora-render-textures` 已确认并落地「无 tier 正交 → 非模板基类 + 子类、`usage` 作 `Init` 参数」的简化方向，buffer 层同理。

约束：`namespace sky::aurora`；无新增第三方；行为语义不变（纯结构重构）；`RawBufferStream` 保留（`aurora-render-textures` 复用）；命名遵守 coding rules（无尾下划线、注释只解释 why）。

## Goals / Non-Goals

**Goals:**

- 移除 kind tag 与「tier × kind 组合」契约。
- `StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 拍平为非模板类（继承 `RenderResource`），用途位 `usage` 改为 `Init` 参数。
- `VertexBuffer`/`IndexBuffer`/`UniformBuffer`/`StorageBuffer` 拍平为非模板具体类，各自 `Init` 固定用途位。
- 同步更新 `RenderGeometry.h` 与两个测试文件。
- 行为语义不变，`AuroraCoreTest` 保持全绿。

**Non-Goals:**

- 不改 `RenderResource` 基类契约。
- 不改 `TransientBuffer` 的 stub 语义（仍无 `TransientBufferPool`，pool 是独立 follow-up）。
- 不解决 `StorageBuffer` 默认 tier 的历史歧义（CS 输出 → vertex/index 语义上宜 GPU_ONLY，但当前默认 dynamic——本 change 保持现状）。
- 不改 RHI 接口、不改 `aurora-upload`。

## Decisions

### D1: 移除 kind tag，用途位改为 Init 参数

删除 `VertexBufferKind`/`IndexBufferKind`/`UniformBufferKind`/`StorageBufferKind`。用途位由具体类型的 `Init` 固定，经 `StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 的 `Init(dev, size, usage[, frames])` 传入，存为成员 `usage`，在 `Create()` 读一次填 `Buffer::Descriptor::usage`。

- **理由**：kind tag 的唯一消费者是「默认模板实参」；没有 `tier × kind` 正交组合，编译期 tag 退化为「把一个常量传给模板」。改成 Init 参数后，用途位与 `size` 一样是「创建时的普通配置」，语义等价但少了 4 个 tag 结构体与一层模板。
- **备选**：保留 kind tag 但去掉模板 —— 被否，tag 没有模板可装饰后只剩空壳。

### D2: StaticBuffer / DynamicBuffer / TransientBuffer 拍平为非模板类

三个 tier 由 `template <typename Kind>` 改为普通类，直接继承 `RenderResource`：

```cpp
class StaticBuffer : public RenderResource {
public:
    StaticBuffer() = default;
    explicit StaticBuffer(const Name &inName) : RenderResource(inName) {}

    bool Init(Device *dev, uint64_t inSize, BufferUsageFlags inUsage);
    bool Upload(const void *data, uint64_t inSize, uint64_t offset = 0) override; // staging
    Buffer  *GetBuffer() const;
    uint8_t *Map();   // nullptr

protected:
    void Create() override;   // GPU_ONLY, usage | TRANSFER_DST
    void Release() override;
    BufferPtr       buffer;
    uint64_t        size  = 0;
    BufferUsageFlags usage = BufferUsageFlagBit::NONE;
};

class DynamicBuffer : public RenderResource {
public:
    DynamicBuffer() = default;
    explicit DynamicBuffer(const Name &inName) : RenderResource(inName) {}

    bool Init(Device *dev, uint64_t inSize, BufferUsageFlags inUsage, uint32_t framesInFlight = 1);
    bool Upload(const void *data, uint64_t inSize, uint64_t offset = 0) override; // = Write
    Buffer  *GetBuffer() const;
    uint8_t *Map();
    bool     Write(const void *data, uint64_t inSize, uint64_t offset = 0);
    void     AdvanceFrame();

protected:
    void Create() override;   // CPU_TO_GPU, ring of numFrames
    void Release() override;
    std::vector<BufferPtr> buffers;
    uint64_t        size      = 0;
    uint32_t        numFrames = 1;
    uint32_t        current   = 0;
    BufferUsageFlags usage    = BufferUsageFlagBit::NONE;
};

class TransientBuffer : public RenderResource {
public:
    TransientBuffer() = default;
    explicit TransientBuffer(const Name &inName) : RenderResource(inName) {}

    bool Init(Device *dev, uint64_t inSize, BufferUsageFlags inUsage);
    bool Upload(const void *data, uint64_t inSize, uint64_t offset = 0) override; // = Map + write
    Buffer  *GetBuffer() const;
    uint8_t *Map();

protected:
    void Create() override;   // CPU_TO_GPU, single buffer (v1; pool is follow-up)
    void Release() override;
    BufferPtr       buffer;
    uint64_t        size  = 0;
    BufferUsageFlags usage = BufferUsageFlagBit::NONE;
};
```

`Create()` 行为与原来一一对应：`StaticBuffer` 单 buffer GPU_ONLY（`usage | TRANSFER_DST`）、`DynamicBuffer` ring `numFrames` 个 CPU_TO_GPU、`TransientBuffer` 单 buffer CPU_TO_GPU。`GetBuffer()`/`Map()`/`Write()`/`AdvanceFrame()`/`Upload()` 逻辑逐行平移，仅把 `Kind::Usage` 换成成员 `usage`。

- **理由**：tier 语义（内存 + 上传策略）真实且彼此不同，保留为三个类；去掉 Kind 模板后 `usage` 自然落为成员，与 `size` 同为创建参数。
- **备选**：三个 tier 合并成一个 `Buffer` 类 + 运行时分支 —— 被否，丢失编译期区分，且 `Map()`/ring 语义只属于 dynamic/transient，合并会引入无效分支。

### D3: 具体类型拍平为非模板类

```cpp
class VertexBuffer : public StaticBuffer {
public:
    VertexBuffer() = default;
    explicit VertexBuffer(const Name &inName) : StaticBuffer(inName) {}

    bool Init(Device *dev, uint64_t size)
    {
        return StaticBuffer::Init(dev, size, BufferUsageFlagBit::VERTEX);
    }
    const VertexLayout &GetLayout() const;
    void SetLayout(const VertexLayout &inLayout);
private:
    VertexLayout layout;
};

class IndexBuffer : public StaticBuffer {
public:
    IndexBuffer() = default;
    explicit IndexBuffer(const Name &inName) : StaticBuffer(inName) {}

    bool Init(Device *dev, uint64_t size)
    {
        return StaticBuffer::Init(dev, size, BufferUsageFlagBit::INDEX);
    }
    IndexType GetIndexType() const;
    void SetIndexType(IndexType inType);
private:
    IndexType indexType = IndexType::NONE;
};

class UniformBuffer : public DynamicBuffer {
public:
    UniformBuffer() = default;
    explicit UniformBuffer(const Name &inName) : DynamicBuffer(inName) {}

    bool Init(Device *dev, uint64_t size, uint32_t framesInFlight = 1)
    {
        return DynamicBuffer::Init(dev, size, BufferUsageFlagBit::UNIFORM, framesInFlight);
    }
};

class StorageBuffer : public DynamicBuffer {
public:
    StorageBuffer() = default;
    explicit StorageBuffer(const Name &inName) : DynamicBuffer(inName) {}

    bool Init(Device *dev, uint64_t size, uint32_t framesInFlight = 1)
    {
        return DynamicBuffer::Init(dev, size,
            BufferUsageFlagBit::STORAGE | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX | BufferUsageFlagBit::INDIRECT,
            framesInFlight);
    }
    Buffer *AsVertex(uint64_t offset) const;
    Buffer *AsIndex(uint64_t offset) const;
    Buffer *AsIndirect(uint64_t offset) const;
};
```

- `VertexBuffer`/`IndexBuffer` 继承 `StaticBuffer`（static 语义 + 解释元数据），`UniformBuffer`/`StorageBuffer` 继承 `DynamicBuffer`（每帧 CPU 写语义），`StorageBuffer` 用途位为 `STORAGE|VERTEX|INDEX|INDIRECT` 超集并保留重解释。
- 调用方写法变化：`VertexBuffer<>` → `VertexBuffer`（非模板，去掉空 `<>`）。`RenderGeometry.h` 的 `std::unique_ptr<VertexBuffer<>>` → `std::unique_ptr<VertexBuffer>`。
- **理由**：与 `aurora-render-textures` 的「基类 + 子类」方向一致；用途位收在子类 `Init` 里，调用方无需关心 usage bit。
- **备选**：具体类型也继承 `RenderResource` 各自实现 —— 被否，会重复 static/dynamic 的 `Create`/上传逻辑；继承 `StaticBuffer`/`DynamicBuffer` 正好复用其行为。

### D4: 不新增名为 `Buffer` 的基类

resource 层**不引入** `Buffer` 类名——`sky::aurora::Buffer` 已是 `rhi::Buffer`（裸 buffer）的命名，同命名空间会冲突。static/dynamic 行为类直接继承 `RenderResource` 即可，无需额外中间基类。

- **理由**：避免与 `rhi::Buffer` 撞名；`StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 的 `Create`/`Release` 本就各自不同，共享基类收益为零。
- **备选**：新增 `BufferBase` 承载 `buffer/size/usage` 公共字段 —— 被否，三个类成员几乎相同但语义独立，抽基类反而多一层且撞名风险仍在。

### D5: 保留 TransientBuffer（拍平，仍为 stub）

`TransientBuffer` 拍平为非模板类保留，语义不变：每帧 scratch buffer，v1 直接经 `Device` 分配单个 CPU_TO_GPU buffer，专用 `TransientBufferPool` 仍是 follow-up（与 RDG `TransientPool` 区分）。

- **理由**：用户确认保留其 spec 契约（「每帧 scratch buffer」语义）；本 change 只做结构拍平，不改变 stub 状态。
- **备选**：删除（0 使用 0 测试）—— 被否，用户选择保留。

## Risks / Trade-offs

- **[用途位从编译期变运行时]** `usage` 由 `static constexpr` 变成员，`static_assert(Kind::Usage & VERTEX)` 这类编译期断言不再可能。→ 缓解：具体类型 `Init` 硬编码用途位，测试改为运行时断言（读回 `GetBuffer()` 后验证 descriptor），行为契约不变。
- **[失去 `tier × kind` 组合表达]** 未来若真需要动态顶点缓冲，需新增 `DynamicVertexBuffer` 类（而非一个模板实参）。→ 缓解：当前 0 使用，YAGNI；真有需求时加一个薄子类即可。
- **[机械重构易遗漏调用方]** `VertexBuffer<>` → `VertexBuffer` 影响 `RenderGeometry.h` 与测试。→ 缓解：tasks 明确列出全部受影响文件，编译 + 测试兜底。

## Migration Plan

1. `Buffer.h` 重写（移除 kind tag + 模板，拍平为 3 个 tier 类 + 4 个具体类）。
2. `RenderGeometry.h`、`RenderGeometryTest.cpp`、`BufferResourceTest.cpp` 同步改 `<>` 调用。
3. 编译 + `AuroraCoreTest` 全绿后 archive。

## Open Questions

- `TransientBufferPool` 何时落地（每帧分配器）——独立 follow-up，本 change 不涉及。
- `StorageBuffer` 默认 tier 是否应为 GPU_ONLY（CS 输出 → vertex/index 语义）——独立 change，本 change 保持现状。
