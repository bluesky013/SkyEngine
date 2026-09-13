# aurora-upload Specification

## Purpose

RHI 上传能力：`DeviceCapability::isUMA` 表达内存拓扑，`Queue::UploadBuffer` / `Queue::UploadImage` 按「内存拓扑 × in-flight 状态」选择上传路径，resource 层无需关心 staging 细节。

## ADDED Requirements

### Requirement: DeviceCapability 暴露 isUMA 内存拓扑

`DeviceCapability` SHALL 新增 `bool isUMA`（默认 `false`），后端 SHALL 在 `UpdateDeviceCaps()` 填充：

- Vulkan：存在 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_COHERENT_BIT` 的 memory heap 时 SHALL 置 `true`；
- DX12：`D3D12_FEATURE_DATA_ARCHITECTURE` 的 `UMA` / `CacheCoherentUMA` 为真时 SHALL 置 `true`；
- Metal：`MTLDevice.hasUnifiedMemory` 为真时 SHALL 置 `true`；
- GLES：SHALL 恒置 `true`。

#### Scenario: 集成 GPU 报 UMA

- **WHEN** 在统一内存架构（Apple Silicon / 移动 SoC / Intel 集显）上初始化 Device
- **THEN** `GetCapability().isUMA == true`

#### Scenario: 独显报非 UMA

- **WHEN** 在独立显存 GPU（NVIDIA/AMD 独显）上初始化 Device
- **THEN** `GetCapability().isUMA == false`

### Requirement: Queue 提供 buffer/image 上传入口

`Queue` SHALL 提供 `UploadBuffer(const BufferPtr&, const std::vector<BufferUploadRequest>&)` 与 `UploadImage(const ImagePtr&, const std::vector<ImageUploadRequest>&)`，复用现有 `IUploadStream` / `BufferUploadRequest` / `ImageUploadRequest`，返回可等待句柄。

#### Scenario: buffer 上传

- **WHEN** 构造 `BufferUploadRequest`（含 `IUploadStream` 源 + offset/size）并调 `Queue::UploadBuffer`
- **THEN** 目标 `rhi::Buffer` 在请求 offset/size 处被写入源数据

### Requirement: per-frame staging ring 按 in-flight frame 回收

上传实现 SHALL 使用 per-frame staging ring（`CPU_TO_GPU`），SHALL 按 in-flight frame 分段，SHALL 只在对应 frame 的 fence 完成后才回收复用，保证写入区不与 GPU 正在读的区重叠。

#### Scenario: staging ring 不覆盖在途数据

- **WHEN** 连续多帧上传，前一帧的 copy 仍在途（fence 未完成）
- **THEN** 后续帧的 staging 分配不落入前一帧仍被 GPU 读取的区段

### Requirement: 上传路径按内存拓扑 × in-flight 状态选择

上传实现 SHALL 按以下规则选择路径：

- 目标**不可 host 直写**（discrete `GPU_ONLY`）→ 一律 staging ring + `CopyBuffer`；
- 目标**host 可见**且**不在途** → 直接 map + memcpy（无 copy）；
- 目标**host 可见**且**在途**（GPU 正在读）→ staging ring + copy（或 fence 等待后直写），避免 CPU/GPU 竞争。

#### Scenario: discrete 走 staging

- **WHEN** 在 `isUMA == false` 的设备上上传一块 `GPU_ONLY` buffer
- **THEN** 数据先写入 staging ring，再 copy 到目标（不直接 map 目标）

#### Scenario: UMA 非在途走直写

- **WHEN** 在 `isUMA == true` 的设备上上传一块「尚未被任何 command 引用」的 host-visible buffer
- **THEN** 直接 map + memcpy，无 staging copy

#### Scenario: UMA 在途走 staging

- **WHEN** 在 `isUMA == true` 的设备上上传一块「被上一帧在途 command 引用」的 host-visible buffer
- **THEN** 走 staging ring + copy（或 fence 等待后直写），不直接覆盖 GPU 正在读的内存

### Requirement: 两种上传模式（异步 upload queue vs 当帧 inline）

`Queue::UploadBuffer` / `UploadImage` SHALL 为**异步**上传：SHALL NOT 阻塞调用线程，SHALL 提交到 transfer/upload 队列并返回 `TransferTaskHandle`；`Queue::Wait(handle)` SHALL 阻塞等待完成，`Queue::HasComplete(handle)` SHALL 返回是否完成。当帧上传 SHALL 经 `StagingBufferAllocator`（每帧 bump 分配，`Allocate(size, align)` 返回 `{buffer, offset, mapped}`），调用方 SHALL 写入后经 `BlitEncoder::CopyBuffer` 内联记录在当前帧 command buffer。

#### Scenario: 异步上传不阻塞

- **WHEN** 调用 `UploadBuffer` 后立即返回
- **THEN** 返回非阻塞 `TransferTaskHandle`，数据在 transfer 队列上后台完成；`HasComplete` 在完成前为 false，`Wait` 后为 true

#### Scenario: 当帧上传内联可见

- **WHEN** 通过 `StagingBufferAllocator` 写入并内联 `CopyBuffer` 到当前帧 command buffer
- **THEN** 同一 command buffer 内的后续绘制可读到该数据（无跨队列同步）

### Requirement: resource 层上传委托 RHI

resource 层（`aurora-render-buffers` 的 `RenderResource::Upload` / `StaticBuffer::Upload`）SHALL 只构造 `BufferUploadRequest` 并调 `Queue::UploadBuffer`，SHALL NOT 内含 staging 逻辑、`isUMA` 分支或 in-flight 判定。

#### Scenario: resource 层无 staging 逻辑

- **WHEN** 阅读 `aurora/resource` 的上传实现
- **THEN** 无 staging buffer 分配、无 `isUMA` 判断、无 in-flight 判定；上传意图全部经 `Queue::UploadBuffer` 委托 RHI
