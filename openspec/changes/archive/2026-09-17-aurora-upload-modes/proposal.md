## Why

`aurora-upload` 已在 RHI 层定义了两种上传模式（异步 `Queue::UploadBuffer/UploadImage` → `TransferTaskHandle`；当帧 inline 经 `StagingBufferAllocator` + `BlitEncoder::CopyBuffer`），但 resource 层（`aurora-render-buffers` / `aurora-render-textures`）并没有清晰地落到这两种模式上，暴露了两个问题：

1. **异步上传忽略完成句柄**：`StaticBuffer::Upload` / `Texture::UploadImage` 调 `Queue::UploadBuffer/UploadImage` 后丢弃 `TransferTaskHandle`，从不 `Wait`。结果是资源在异步拷贝仍在途时就被销毁——测试中已观测到 `vkDestroyBuffer` / `vkDestroyImage` 校验错误（`currently in use by VkCommandBuffer`）。
2. **当帧 staging 上传路径缺失**：resource 层只有异步（transfer queue）与持久映射（`DynamicBuffer` 的 CPU_TO_GPU 直写）两种，没有「当帧 staging 上传」——即用 `StagingBufferAllocator`（每帧 bump ring，随 `DeviceFrameContext` 的 in-flight 帧驱动）+ 内联 `BlitEncoder::CopyBuffer` 把每帧数据拷进 GPU_ONLY 目标。

本 change 重新审视 texture / buffer 的上传流程，把它收敛为两种模式，并补齐两个缺口。

## What Changes

- **明确两种上传模式**：
  - **异步上传（transfer queue + staging）**：`StaticBuffer`、`Texture`（v1 纹理一律 GPU_ONLY）→ `Queue::UploadBuffer/UploadImage`，异步非阻塞，追踪 `TransferTaskHandle`。
  - **当帧 staging 上传（frame context）**：新增 `FrameStagingBuffer`（resource 层，`aurora/resource/FrameStagingBuffer.h`）——包装 `StagingBufferAllocator`（每帧 bump ring，`numFrames` 对齐 `DeviceFrameContext::inflightNum`），`Upload(dst, data, size)` 写入 staging 槽并排队，`Flush(BlitEncoder&)` 在当前帧 command buffer 内联 `CopyBuffer` 到 GPU_ONLY 目标，`Reset()` 随 frame context 每帧推进。
- **修复异步完成缺口**：`StaticBuffer` / `Texture` 追踪 pending `TransferTaskHandle` + 提交它的 `Queue*`，新增 `WaitUploadComplete()` / `IsUploadComplete()`；析构（destructor）前 SHALL 等待 pending 上传，避免在途销毁。
- **明确 `DynamicBuffer`/`TransientBuffer` 的 ring 与 frame context 锁步契约**（持久映射直写，属第三种机制而非「两种上传模式」之一）：`framesInFlight` SHALL 等于 `inflightNum`，`AdvanceFrame()` 由 frame driver 每帧驱动。
- texture 侧 v1 **只支持异步上传**（无当帧 staging 纹理上传）；当帧 staging 纹理上传留待后续。

## Capabilities

### New Capabilities

（无 —— 本 change 是既有上传能力的契约澄清 + 完成语义补齐 + 当帧 staging 上传路径落地。）

### Modified Capabilities

- `aurora-render-buffers`: `StaticBuffer` 异步上传增加完成同步（`WaitUploadComplete`/`IsUploadComplete` + 析构前等待）；新增 `FrameStagingBuffer` 当帧 staging 上传；`DynamicBuffer`/`TransientBuffer` 的 ring 与 `DeviceFrameContext` 锁步契约。
- `aurora-render-textures`: `Texture` 异步上传（`Upload`/`UploadImage`）增加完成同步，析构前等待 pending 上传。

## Impact

- **新增文件**：`engine/aurora/core/include/aurora/resource/FrameStagingBuffer.h`（header-only，包装 `StagingBufferAllocator` + 内联 copy 队列）。
- **修改文件**：`engine/aurora/core/include/aurora/resource/Buffer.h`（`StaticBuffer` 追踪 handle + 完成 API + 析构等待）、`engine/aurora/core/include/aurora/resource/Texture.h`（`Texture` 追踪 handle + 完成 API + 析构等待）。
- **测试**：`engine/aurora/core/test/BufferResourceTest.cpp` 增加 `FrameStagingUpload`（staging 上传 + 内联 copy + 读回验证）与完成语义断言；`TextureResourceTest.cpp` 增加完成语义断言。
- **依赖**：`aurora/rhi`（`Queue::UploadBuffer/UploadImage` + `TransferTaskHandle` + `Wait/HasComplete`、`StagingBufferAllocator`、`BlitEncoder::CopyBuffer`，均已存在）、`aurora-upload`（两种模式契约，已存在）。
- **不影响**：`DeviceFrameContext` / RHI 接口层；`RenderGeometry`；`TextureAtlas`（复用 `Texture::UploadImage`，自动受益）。
