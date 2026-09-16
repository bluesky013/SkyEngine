## 1. Buffer.h 重写（移除 kind tag + 模板）

- [x] 1.1 删除 `VertexBufferKind`/`IndexBufferKind`/`UniformBufferKind`/`StorageBufferKind` 四个 kind tag
- [x] 1.2 `StaticBuffer` 拍平为非模板类（继承 `RenderResource`）：`Init(dev, size, usage)` + 成员 `usage`，`Create()` 用 `usage | TRANSFER_DST`（GPU_ONLY），`Upload`/`Map()=nullptr` 平移
- [x] 1.3 `DynamicBuffer` 拍平为非模板类：`Init(dev, size, usage, frames)`，`Create()` ring（CPU_TO_GPU），`Map`/`Write`/`AdvanceFrame` 平移
- [x] 1.4 `TransientBuffer` 拍平为非模板类：`Init(dev, size, usage)`，单 buffer（CPU_TO_GPU），stub 语义不变

## 2. 具体类型拍平

- [x] 2.1 `VertexBuffer`/`IndexBuffer` 非模板继承 `StaticBuffer`，`Init(dev, size)` 固定 `VERTEX`/`INDEX`，保留 `VertexLayout`/`IndexType`
- [x] 2.2 `UniformBuffer`/`StorageBuffer` 非模板继承 `DynamicBuffer`，`Init(dev, size, frames)` 固定 `UNIFORM`/超集 usage，保留 `AsVertex`/`AsIndex`/`AsIndirect`
- [x] 2.3 保留 `RawBufferStream`（非模板，`aurora-render-textures` 复用）

## 3. 同步调用方

- [x] 3.1 `RenderGeometry.h`：`std::unique_ptr<VertexBuffer<>>`/`IndexBuffer<>` → `std::unique_ptr<VertexBuffer>`/`IndexBuffer`（去 `<>`）
- [x] 3.2 `RenderGeometryTest.cpp`：`VertexBuffer<>`/`IndexBuffer<>` → 非模板
- [x] 3.3 `BufferResourceTest.cpp`：移除 kind tag `static_assert`，改为运行时断言具体类型用途位；`VertexBuffer<>`/`IndexBuffer<>`/`UniformBuffer<>`/`StorageBuffer<>` → 非模板

## 4. 验证与收尾

- [x] 4.1 `cmake --build` AuroraCore 通过
- [x] 4.2 `AuroraCoreTest` 全绿
- [ ] 4.3 `openspec archive aurora-render-buffers-simplify` 归档（需用户确认）
