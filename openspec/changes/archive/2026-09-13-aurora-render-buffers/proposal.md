## Why

Aurora 目前只有底层 `rhi::Buffer` / `rhi::Image`（`aurora/rhi/`）——仅含 `Descriptor` 与 `Map/UnMap` 的裸资源，4 个后端各有一份 `*Buffer`/`*Image` 实现。调用方要用一块 Vertex/Index/Uniform/Storage 缓冲，必须自己拼 `BufferUsageFlagBit`、选 `MemoryType`、手写 map/unmap 与上传逻辑，且没有任何「static（一次上传不可变）/ dynamic（每帧 CPU 写）/ transient（帧内临时）」的生命周期语义，也没有任何统一的资源管理（惰性创建、上传、统计）。渲染层（`GlobalRenderResources`、`PipelinePass`、RDG batch tier）都需要一个比裸 `rhi::Buffer` 更高的资源层：统一封装 rhi 资源、按需惰性创建、统一上传、可被统计管理。

## What Changes

- 在 `aurora/core` 模块内新增 **`aurora/resource`** 子目录（`include/aurora/resource/` + `src/resource/`），引入统一的 **`RenderResource`** 基类：它是 **buffer 与 image 资源的公共基类**，作用是对 rhi 资源的一次封装，提供两件事——
  1. **惰性创建**：持有 `rhi::Device*` 与 descriptor，实际 `rhi::Buffer`/`rhi::Image` 在真正需要时（首次访问）才创建；
  2. **统一上传**：`Upload(data, size, offset)` 把 host 数据上传到 device——上传策略（staging vs 直接写、UMA vs discrete、in-flight 判定）由 RHI 的 `aurora-upload` 能力决定，resource 层只声明上传意图。
- 引入 **访问/生命周期 tier 模板接口**（buffer 侧，均继承 `RenderResource`）：`template <typename Kind> StaticBuffer`（GPU_ONLY，`Upload` 委托 `Queue::UploadBuffer`）、`template <typename Kind> DynamicBuffer`（CPU_TO_GPU，**CPU 上传** + 同步 inflight frame）、`template <typename Kind> TransientBuffer`（每帧内使用，经 `TransientBufferPool` **每帧分配**，与 RDG `TransientPool` 严格区分）。
- 引入**带解释元数据的 buffer 具体类型**（模板化于 storage tier，带默认 tier）：
  - **`VertexBuffer`**：携带 `VertexLayout`（`stride` + `VertexSemanticMask` 语义 + `VertexInputRate`），供 `BindVertexBuffers` 使用。
  - **`IndexBuffer`**：携带 `IndexType`（`U16`/`U32`），供 `BindIndexBuffer` 使用。
  - **`UniformBuffer`**：UBO 视图。
  - **`StorageBuffer`**：**通用** buffer——usage 为 `STORAGE | VERTEX | INDEX | INDIRECT` 超集，既可作为 SSBO，也可作为 CS 输出结果后被重新解释为 vertex / index / indirect buffer。
- 提供最小可用测试（`aurora/core/test/BufferResourceTest.cpp`）：验证惰性创建、上传、tier 语义、Vertex/Index 解释元数据、StorageBuffer 重解释。

> 资源/显存统计（实际创建了多少 image、分类占用多少显存）由**后端**负责——Vulkan 用 VMA、DX12 用 D3D12MA 做显存池，统计只有后端知道；本 change 不设 `RenderResourceManager`。

## Capabilities

### New Capabilities

- `aurora-render-buffers`: aurora 渲染层资源抽象——统一 `RenderResource` 基类（buffer/image 公共基类，惰性创建 + 上传）、static/dynamic/transient 三档 tier 模板，以及带解释元数据的 VertexBuffer / IndexBuffer / UniformBuffer / 通用 StorageBuffer。（资源/显存统计归后端，不在此能力内）

### Modified Capabilities

（无既有 spec 修改）

## Impact

- **新增文件**：`engine/aurora/core/include/aurora/resource/RenderResource.h`、`.../resource/Buffer.h`（kind tag + tier 模板 + Vertex/Index/Uniform/Storage 具体类型）、`engine/aurora/core/src/resource/`（非模板实现：transient 池、上传委托）。
- **依赖**：`aurora/rhi`（`CreateBuffer/CreateImage`、`Buffer`、`Image`、`MemoryType`、`BufferUsageFlagBit`、`IndexType`、`VertexSemantic`）、`core`（`RefObject`、`Name`），以及 **`aurora-upload` change**（`StaticBuffer::Upload` 委托 `Queue::UploadBuffer`）、**`aurora-resource-name` change**（`RenderResource::name_` 受 `SKY_ENABLE_RESOURCE_NAME` 守卫并下传后端 label）。
- **模块边界**：本 change **不修改 `aurora/rhi`**；上传能力由独立的 `aurora-upload` change 提供。
- **调用方**：暂无；本 change 只落地抽象与测试，后续 change 再接入 `GlobalRenderResources` / `PipelinePass` 与 image 资源类型。
