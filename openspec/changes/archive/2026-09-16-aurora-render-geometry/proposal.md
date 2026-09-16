## Why

`aurora-render-buffers` 已落地 `RenderResource` 基类 + `StaticBuffer/DynamicBuffer/TransientBuffer` 三档 tier + `VertexBuffer`/`IndexBuffer` 元数据封装。但还缺一层「网格几何」的复合资源：把顶点流 + 索引缓冲 + 本地包围盒组合成一个可整体上传、可被 mesh 实例复用的对象。

当前 scene 组件只有 `Bounds` / `WorldInfo` / `Light` / `Skin`，没有承载几何数据的地方；`Renderer.h` 也是 TODO。要往下走 mesh 渲染，第一步就是把 `RenderGeometry` 封装出来，作为 `RenderResource` 之上的「复合网格资源」。

## What Changes

- **新增 `RenderGeometry`**（`aurora/resource/RenderGeometry.h`，header-only）：`RefObject` 复合资源，持有：
  - 顶点流：`std::vector<VertexBuffer>`（每个自带 `VertexLayout`：stride + `VertexSemanticMask` + `VertexInputRate`）
  - 索引缓冲：可选 `IndexBuffer`（自带 `IndexType`）
  - 本地包围盒：`AABB localBounds`（供 culling）
- **组合上传**：`Upload()` 委托各顶点流 / 索引缓冲逐个上传；提供访问器（`GetVertexStreams()` / `GetIndexBuffer()` / `GetLocalBounds()`）供后续 `DrawItem` / encoder 绑定使用。
- **复用既有 buffer 封装**：不重复实现 vertex/index 缓冲，直接复用 `VertexBuffer` / `IndexBuffer`（默认 `StaticBuffer` tier，动态几何用显式 tier 模板实参）。

## Capabilities

### New Capabilities

- `aurora-render-geometry`: `RenderGeometry` 复合网格资源 —— 顶点流/索引/包围盒的组合、组合上传、访问器契约。

### Modified Capabilities

（无 —— 复用 `aurora-render-buffers` 的 `VertexBuffer`/`IndexBuffer`/`VertexLayout`，不新增 buffer 语义。）

## Impact

- **新文件**：`engine/aurora/core/include/aurora/resource/RenderGeometry.h`（header-only，随 `aurora/core` GLOB 自动纳入）。
- **测试**：`engine/aurora/core/test/` 新增 `RenderGeometryTest.cpp`（编译 + 元数据 + 上传 smoke）。
- **不影响**：`aurora-render-buffers` 既有 buffer 类型与测试；RHI/RDG 接口层。
