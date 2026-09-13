## Why

Aurora 的 RHI 目前没有上传（upload）能力：`IUploadStream` / `BufferUploadRequest` / `ImageUploadRequest` 这些类型已经躺在 `aurora/rhi/Core.h`，但没有任何 `Queue` 方法消费它们，四个后端也没有 staging/上传实现。调用方（未来的 `aurora/resource` 层）要把 host 数据送上 device，必须自己拼 staging buffer + map/unmap + `CopyBuffer`，并自己处理内存拓扑（UMA vs discrete）与 in-flight 同步。

上传路径不是一个能靠调用方拍脑袋决定的事：**即使 UMA 也可能需要走 staging**——因为 GPU 可能正在绘制（in-flight command），CPU 直接写一块 GPU 正在读的内存是数据竞争。discrete 的目标 buffer 通常 `GPU_ONLY` 不可 host 直写，只能 staging + copy；UMA 的目标 host-visible，但只有「目标不在途」时才能直接写。这些判断都依赖后端的内存属性与 per-frame fence，应下沉到 RHI。

## What Changes

- **`DeviceCapability::isUMA`**（默认 `false`），后端在 `UpdateDeviceCaps()` 填充：
  - Vulkan：存在 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | HOST_VISIBLE_BIT | HOST_COHERENT_BIT` 的 heap（UMA/ReBAR）；
  - DX12：`D3D12_FEATURE_DATA_ARCHITECTURE` 的 `UMA`/`CacheCoherentUMA`；
  - Metal：`MTLDevice.hasUnifiedMemory`；
  - GLES：恒 `true`。
- **`Queue::UploadBuffer` / `Queue::UploadImage`**（复用现有 `IUploadStream` / `BufferUploadRequest` / `ImageUploadRequest`），把 host 数据上传到目标资源，返回句柄可等待。
- **per-frame staging ring**：按 in-flight frame 分段、经 frame fence 回收的 staging buffer，保证写入区永远不与 GPU 在读的区重叠。
- **in-flight 感知的上传路径选择**（非简单 UMA=直写）：
  - 目标**不可 host 直写**（discrete `GPU_ONLY`）→ 一律 staging ring + `CopyBuffer`；
  - 目标**host 可见**且**不在途** → 直接 map + memcpy（无 copy）；
  - 目标**host 可见**且**在途**（GPU 正在读）→ staging ring + copy（或 fence 等待后直写），避免 CPU/GPU 竞争。
- 测试：isUMA 上报、staging ring 回收、UMA 在途/非在途路径、discrete staging 路径。

## Capabilities

### New Capabilities

- `aurora-upload`: RHI 上传能力——`DeviceCapability::isUMA` + `Queue::UploadBuffer`/`UploadImage` + per-frame staging ring，按「内存拓扑 × in-flight 状态」选择上传路径（staging vs 直接写）。

### Modified Capabilities

（无既有 spec 修改）

## Impact

- **新增/修改文件**：`aurora/rhi/Core.h`（`DeviceCapability::isUMA`）、`aurora/rhi/Queue.h`（上传方法）、4 后端 `UpdateDeviceCaps()` 与 `Queue::Upload*` 实现、staging ring 管理（接口层或后端）。
- **依赖**：`aurora-frame-context`（per-frame fence / 在途判定）、`aurora-encoder-barriers`（copy 前后 barrier）；`aurora-queue-submit-present`（如需异步 submit 等待）。
- **调用方**：暂无；`aurora-render-buffers` 的 `StaticBuffer::Upload` 将委托本能力。
- **边界**：属于 `aurora/rhi`，为 resource 层提供统一上传入口；resource 层不接触 staging 细节。
