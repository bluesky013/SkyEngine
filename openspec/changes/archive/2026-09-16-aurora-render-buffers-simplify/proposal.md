## Why

`aurora-render-buffers` 落地时采用了「kind tag + storage tier 模板 + 具体类型模板化于 Storage」的三层结构，其设计初衷是表达 `tier × kind` 的全组合（如 `VertexBuffer<DynamicBuffer<VertexBufferKind>>`）。但落地至今该正交组合**从未被使用**：

- 所有调用都是默认组合（`VertexBuffer<>`/`IndexBuffer<>` → static，`UniformBuffer<>`/`StorageBuffer<>` → dynamic）。
- `StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 没有任何带显式 `Kind` 的实例化。
- `Kind` tag 的全部内容是一行 `static constexpr BufferUsageFlags Usage`，只被 3 处 `Create()` 消费（`desc.usage = Kind::Usage`），本质只是「把用途位从默认模板实参传给 Create」。
- `TransientBuffer` 更是 0 使用、0 测试的 stub。

即：kind 维度是冗余的——每个具体类型本身就是一种 kind（VertexBuffer=VERTEX、IndexBuffer=INDEX…），`Kind` tag 只是把这个已知值绕一圈再传回给 tier。这是死重量：4 个 tag + 3 个 tier 模板 + 4 个模板化具体类型，为一种没人用的组合能力买单，且会误导读者以为「任意 tier × kind 组合」都被支持。

`aurora-render-textures` 已确认并落地「无 tier 正交 → 非模板基类 + 子类、`usage` 作 `Init` 参数」的简化方向；buffer 层同理成立，应一并简化。

## What Changes

- **移除 4 个 kind tag**（`VertexBufferKind`/`IndexBufferKind`/`UniformBufferKind`/`StorageBufferKind`）与「tier × kind 组合可表达」契约。
- **`StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 拍平为非模板类**，直接继承 `RenderResource`，用途位 `BufferUsageFlags` 改为 `Init(dev, size, usage[, frames])` 的运行时参数（存成员，`Create()` 读一次）。
- **`VertexBuffer`/`IndexBuffer`/`UniformBuffer`/`StorageBuffer` 拍平为非模板具体类**，分别继承 `StaticBuffer`（Vertex/Index）或 `DynamicBuffer`（Uniform/Storage），各自 `Init` 里固定用途位（VERTEX / INDEX / UNIFORM / STORAGE|VERTEX|INDEX|INDIRECT）。
- **`TransientBuffer` 保留并拍平**（维持「每帧 scratch buffer」语义与 spec 契约，仍为无池 stub）。
- **同步更新调用方**：`RenderGeometry.h` 的 `VertexBuffer<>`/`IndexBuffer<>` → `VertexBuffer`/`IndexBuffer`，以及 `BufferResourceTest.cpp`/`RenderGeometryTest.cpp`。
- 行为语义**不变**：static 仍 GPU_ONLY + staging 上传 + `Map()=nullptr`；dynamic 仍 CPU_TO_GPU + 持久映射 + ring；StorageBuffer 仍可 `AsVertex`/`AsIndex`/`AsIndirect` 重解释。

## Capabilities

### New Capabilities

（无 —— 本 change 是对既有能力的结构重构，不引入新能力。）

### Modified Capabilities

- `aurora-render-buffers`: 移除 kind tag 与「tier × kind 组合」契约；`StaticBuffer`/`DynamicBuffer`/`TransientBuffer` 与 `VertexBuffer`/`IndexBuffer`/`UniformBuffer`/`StorageBuffer` 由模板改为非模板类，用途位由 `Init` 参数承载。

## Impact

- **修改文件**：`engine/aurora/core/include/aurora/resource/Buffer.h`（kind tag + tier 模板 + 具体类型全部重写为非模板）、`engine/aurora/core/include/aurora/resource/RenderGeometry.h`（`VertexBuffer<>`/`IndexBuffer<>` → 非模板）、`engine/aurora/core/test/BufferResourceTest.cpp`、`engine/aurora/core/test/RenderGeometryTest.cpp`。
- **依赖**：`aurora/rhi`（`Buffer`/`BufferUsageFlagBit`/`MemoryType`/`Queue::UploadBuffer`）、`core`（`RefObject`、`Name`）、`aurora-upload`（上传能力，不变）。
- **不影响**：`RenderResource` 基类契约、`aurora-upload`、RHI 接口层；`aurora-render-textures` 复用 `Buffer.h` 的 `RawBufferStream`（非模板，不受影响）。
