## Context

`aurora-render-buffers` 已落地：

- `RenderResource` 基类：惰性 `Create()/Release()` + 统一 `Upload(data, size, offset)`，`name`/`device`/`created`。
- buffer tier：`StaticBuffer<Kind>`（GPU_ONLY 一次性）/ `DynamicBuffer<Kind>`（CPU_TO_GPU ring）/ `TransientBuffer<Kind>`（每帧池）。
- 元数据封装：`VertexBuffer`（`VertexLayout`：stride + `VertexSemanticMask` + `VertexInputRate`）、`IndexBuffer`（`IndexType`）。
- `VertexSemanticMask`：固定 13 种语义的 16-bit 位掩码（`aurora/rhi/VertexSemantic.h`）。

缺「网格几何」复合层：顶点流 + 索引 + 包围盒的组合，作为 mesh 实例可复用的 render 资源。scene 侧没有几何组件，`Renderer.h` 是 TODO。

约束：`namespace sky::aurora`；无新增第三方；复用既有 buffer 封装，不重复造 vertex/index buffer。

## Goals / Non-Goals

**Goals:**

- `RenderGeometry`：`RefObject` 复合资源，持有顶点流（多 stream）+ 索引缓冲（可选）+ 本地 `AABB`，纯封装。
- 访问器：供后续 `DrawItem` / `GraphicsEncoder` 绑定（vb/ib 指针 + 布局）。

**Non-Goals:**

- 不做「从数据构建 + 上传」的便捷层（上传是调用方 + `VertexBuffer`/`IndexBuffer` 自身的职责；如需 builder 另立 `GeometryBuilder`）。
- 不做 `RenderMesh`（geometry + material + sub-mesh 段），material/材质是后续 change。
- 不做绑定到 `DrawItem` / encoder 的实际接线（属 renderer/technique 层）。
- 不改 `aurora-render-buffers` 的 buffer 类型与契约。

## Decisions

### 1. `RenderGeometry` 不继承 `RenderResource`，是复合 `RefObject`，纯封装

`RenderResource` 是「单个 rhi 资源（buffer/image）的封装」（`Create()/Release()` 对单个底层资源）。`RenderGeometry` 组合多个 buffer，语义不同，故不继承，而是 `RefObject`（多 mesh 共享同一几何数据时用 `CounterPtr`）。

geometry 是**纯封装**：不持有 host 数据、不提供 `Upload()`（buffer 的 `Upload(data,size)` 需要数据，geometry 拿不出）。调用方先创建并上传 `VertexBuffer`/`IndexBuffer`，再移交 geometry。

```cpp
class RenderGeometry : public RefObject {
public:
    RenderGeometry() = default;
    explicit RenderGeometry(const Name &name) : name(name) {}
    ~RenderGeometry() override = default;

    RenderGeometry(const RenderGeometry &) = delete;
    RenderGeometry &operator=(const RenderGeometry &) = delete;

    void SetLocalBounds(const AABB &bounds) { localBounds = bounds; }

    void AddVertexStream(std::unique_ptr<VertexBuffer<>> vb);
    void SetIndexBuffer(std::unique_ptr<IndexBuffer<>> ib);

    const std::vector<std::unique_ptr<VertexBuffer<>>> &GetVertexStreams() const;
    IndexBuffer<> *GetIndexBuffer() const;
    const AABB &GetLocalBounds() const;
    const Name &GetName() const;

private:
    Name name;
    std::vector<std::unique_ptr<VertexBuffer<>>> vertexStreams;
    std::unique_ptr<IndexBuffer<>> indexBuffer;
    AABB localBounds{};
};
```

**备选**：继承 `RenderResource` → 未采用，`RenderResource` 的 `Create/Release` 是单资源生命周期，geometry 无单一底层资源。

### 2. buffer 所有权用 `std::unique_ptr`（不造 `Ptr` 别名）

现有 `VertexBuffer<>`/`IndexBuffer<>` 继承 `RenderResource`（非 RefObject），不可 `CounterPtr`。geometry 用 `std::unique_ptr<VertexBuffer<>>` / `std::unique_ptr<IndexBuffer<>>` 独占持有：

- **不造 `VertexBufferPtr` 别名**：仓库里 `XxxPtr` 约定是 `CounterPtr<Xxx>`（ref-counted 共享）；`unique_ptr` 独占语义用 `Ptr` 后缀会误导。
- `AddVertexStream(std::unique_ptr<VertexBuffer<>>)` / `SetIndexBuffer(std::unique_ptr<IndexBuffer<>>)` 转移所有权（`std::move`）。
- geometry 析构时自动释放底层 `BufferPtr`。

动态几何（`VertexBuffer<DynamicBuffer<VertexBufferKind>>`）是不同模板实例，无法进同一 `std::vector` —— 本 change 只支持 static 顶点流，动态几何留后续。

**备选**：按值 `std::vector<VertexBuffer<>>`（copy 时共享底层 BufferPtr）→ 未采用，语义隐晦（浅拷贝共享底层 buffer），`unique_ptr` 所有权更清晰。

### 3. header-only，随 `aurora/core` GLOB

`RenderResource`/`Buffer.h` 都是 header-only；`RenderGeometry` 沿用（`aurora/core/include/aurora/resource/RenderGeometry.h`），无需新 `.cpp`，CMake 无需改。

### 4. 包围盒用 `AABB`（本地空间）

scene `Bounds` 组件已用 `core/shapes/AABB`。geometry 存 `AABB localBounds`（本地/模型空间），世界空间包围盒由 `WorldInfo` 变换得到，不做重复存储。

### 5. 绑定留到 renderer

`RenderGeometry` 只提供访问器（vb/ib 指针 + 布局），不直接绑定 encoder。绑定路径（`DrawItem.vb/ib` → `GraphicsEncoder::BindVertexBuffers`）由 `aurora-renderer` / scene collect 消费。geometry 保持纯数据，不耦合命令录制，也不解析 per-attribute offset/format（留 shader-derived 顶点输入阶段）。

## Risks / Trade-offs

- **[vertex 流只支持 static tier]** v1 仅 `VertexBuffer<>`（static）；动态几何（顶点每帧更新）需要 `DynamicRenderGeometry` 或模板化 geometry。→ 缓解：明确 Non-Goal，后续 change 扩展。
- **[`unique_ptr` 所有权让 geometry 不可拷贝]** geometry 持 `unique_ptr`，不可拷贝，只能移动/共享（`CounterPtr<RenderGeometry>`）。→ 缓解：正是预期（多 mesh 共享用 `CounterPtr`）。
- **[纯封装无上传]** 上传职责在调用方 + buffer 自身，geometry 不参与。→ 缓解：契约写明，避免「组合上传」的自相矛盾（buffer `Upload` 需要数据）。

## Migration Plan

1. `RenderGeometry.h` 落地。
2. `RenderGeometryTest.cpp`：编译 + 元数据 + 构造/所有权 smoke。
3. archive。
