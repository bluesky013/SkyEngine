# aurora-render-geometry Specification

## Purpose
TBD - created by archiving change aurora-render-geometry. Update Purpose after archive.
## Requirements
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

