## 1. RenderResource 基类（资源封装：惰性创建 + 上传）

- [x] 1.1 新增 `engine/aurora/core/include/aurora/resource/RenderResource.h`（`Device*` device、`Name` name、`bool` created，`GetName()`/`GetDevice()`/`IsCreated()`、纯虚 `Upload`、protected 纯虚 `Create()`/`Release()`）
- [x] 1.2 头文件 ASCII-only、`#pragma once`、命名空间 `sky::aurora`；不持有 `rhi::BufferView`/offset/range；不设自建统计

## 2. buffer kind tag

- [x] 2.1 新增 `engine/aurora/core/include/aurora/resource/Buffer.h`：`VertexBufferKind`/`IndexBufferKind`/`UniformBufferKind`/`StorageBufferKind` + `static constexpr BufferUsageFlags Usage`
- [x] 2.2 `StorageBufferKind::Usage` 超集 `STORAGE|VERTEX|INDEX|INDIRECT`

## 3. tier 模板

- [x] 3.1 `StaticBuffer<Kind>`：`GPU_ONLY`（`|TRANSFER_DST`）惰性创建，`Upload` 构造 `BufferUploadRequest` + `RawBufferStream` 委托 `Queue::UploadBuffer`，`Map()` 返回 nullptr
- [x] 3.2 `DynamicBuffer<Kind>`：`CPU_TO_GPU` 惰性创建，`Map()`/`Write(data,size,offset)`；inflight 同步（ring/fence）待后续
- [x] 3.3 `TransientBuffer<Kind>`：`CPU_TO_GPU` 惰性创建（v1 直接 Device 分配，专用 `TransientBufferPool` 待后续）

## 4. 具体 buffer 类型（storage tier 模板 + 解释元数据）

- [x] 4.1 `VertexLayout`：`stride` + `VertexSemanticMask` + `VertexInputRate`
- [x] 4.2 `VertexBuffer`（默认 `StaticBuffer<VertexBufferKind>`）：携带 `VertexLayout`，`GetLayout()`/`SetLayout()`
- [x] 4.3 `IndexBuffer`（默认 `StaticBuffer<IndexBufferKind>`）：携带 `IndexType`，`GetIndexType()`/`SetIndexType()`
- [x] 4.4 `UniformBuffer`（默认 `DynamicBuffer<UniformBufferKind>`）
- [x] 4.5 `StorageBuffer`（默认 `DynamicBuffer<StorageBufferKind>`）：`AsVertex`/`AsIndex`/`AsIndirect` 返回底层 `Buffer*`

## 5. TransientBufferPool（非模板实现，与 RDG TransientPool 区分）

- [ ] 5.1 专用 `TransientBufferPool`（每帧分配器）——待后续；v1 `TransientBuffer` 直接经 Device 分配
- [ ] 5.2 预留「外部 allocator 注入」钩子

## 6. 测试

- [x] 6.1 新增 `engine/aurora/core/test/BufferResourceTest.cpp`：kind→usage `static_assert`（含 Storage 超集）
- [x] 6.2 具体类型实例化 + VertexLayout 元数据 + IndexType + StorageBuffer 重解释（`AsVertex` 返回 nullptr）
- [ ] 6.3 实际 Device 创建/上传的端到端测试（依赖运行环境，待后续）
- [x] 6.4 构建并运行 `AuroraCoreTest`（`BufferResourceTest` 2 用例通过）

## 7. 构建与风格校验

- [x] 7.1 `Core`/`Aurora.RHI`/`Aurora`/`AuroraVulkan`/`AuroraDX12`/`AuroraCoreTest` 均编译通过（Metal 为 macOS 专属未验证）
- [ ] 7.2 运行 clang-format / clang-tidy（待后续统一执行）
