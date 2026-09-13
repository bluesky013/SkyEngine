## Context

Aurora 的 RHI 接口层（`aurora/rhi/interface`）已有 `IUploadStream`（`Data(offset)`/`ReadData(...)` 的源数据抽象）、`BufferUploadRequest`、`ImageUploadRequest`（`aurora/rhi/Core.h`），但没有任何 `Queue` 方法消费它们，也没有 staging / 上传实现——这些类型是旧引擎 `engine/render/backend/rhi` 迁过来时留下的「未接线」接口。

旧引擎的上传（`engine/render/backend/vulkan/Queue.cpp::UploadBuffer`）已经是 per-frame staging ring + `Copy`，但它是**无条件 staging**，没有区分 UMA/discrete，也没有把「目标是否在途」纳入决策。

本 change 把上传能力真正落到 aurora RHI：`isUMA` capability + `Queue::UploadBuffer/Image` + per-frame staging ring，并采用**「内存拓扑 × in-flight 状态」**的路径选择——这是对上一条「UMA 直写」的修正：即使 UMA，目标在途时也不能直接写。

## Goals / Non-Goals

**Goals:**

- `DeviceCapability::isUMA` 表达内存拓扑，后端在 `UpdateDeviceCaps()` 填充。
- `Queue::UploadBuffer` / `Queue::UploadImage` 作为统一上传入口（复用现有 request 类型）。
- per-frame staging ring：按 in-flight frame 分段、经 frame fence 回收，保证写区不与 GPU 读区重叠。
- in-flight 感知的路径选择：discrete 一律 staging；UMA 仅在目标不在途时直写，在途则 staging（或 fence 等待）。
- 最小测试：isUMA 上报、staging ring 回收、UMA 在途/非在途、discrete staging。

**Non-Goals:**

- 不做异步上传队列/后台线程（v1 同步提交 + 可等待句柄）。
- 不做 image 的完整 row/slice 布局优化（复用 `ImageUploadRequest` 已有字段）。
- 不接入 resource 层（`aurora-render-buffers` 是独立 change，依赖本 change）。
- 不引入新 CMake 模块。

## Decisions

### D1: isUMA 作为 `DeviceCapability` 字段

`DeviceCapability` 新增 `bool isUMA = false`，后端在 `UpdateDeviceCaps()` 填充：

| 后端 | 判定 |
|---|---|
| Vulkan | 存在 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT \| HOST_VISIBLE_BIT \| HOST_COHERENT_BIT` 的 heap（UMA / ReBAR） |
| DX12 | `D3D12_FEATURE_DATA_ARCHITECTURE` 的 `UMA` / `CacheCoherentUMA` |
| Metal | `MTLDevice.hasUnifiedMemory` |
| GLES | 恒 `true` |

- **理由**：符合 AGENTS.md「新增 capability 字段在 `UpdateDeviceCaps()` 写」的约定；拓扑是静态属性，一次性上报。
- **备选**：运行时查 `Buffer::Map() != nullptr` 反推 host-visible —— 被否，语义含糊（`Map` 返回 null 不总等于 device-local），且无法区分「目标是否在途」。

### D2: 上传作为 Queue 方法（transfer 队列）

```cpp
class Queue {
public:
    virtual TransferTaskHandle UploadBuffer(const BufferPtr &buffer,
                                            const std::vector<BufferUploadRequest> &requests) = 0;
    virtual TransferTaskHandle UploadImage(const ImagePtr &image,
                                           const std::vector<ImageUploadRequest> &requests) = 0;
};
```

- **理由**：上传是 transfer 队列上的复制操作，与旧引擎一致；返回可等待句柄，调用方可决定何时同步。
- **备选**：放在 `Device` 上 —— 被否，上传天然有队列归属（transfer），Device 无队列上下文。

### D3: per-frame staging ring（正确性的核心）

每个 in-flight frame 一段 staging buffer（`CPU_TO_GPU`），`Allocate(size, align)` 返回映射指针；该段只在对应 frame 的 fence 信号后回收复用。

- **理由**：staging ring 是保证「写区不与 GPU 读区重叠」的机制——同一段内存只有其 frame fence 完成后才可能被再次写入，从而天然规避 in-flight 竞争。旧引擎 Vulkan 后端已是此结构（`stagingBuffers[currentFrameId]` + `vkQueueWaitIdle`）。
- **备选**：每上传一次新建临时 staging —— 被否，频繁分配/释放，且无法表达 in-flight 复用。

### D4: 路径选择 = 内存拓扑 × in-flight 状态

| 目标状态 | discrete（目标 `GPU_ONLY`，不可 host 直写） | UMA（目标 host-visible） |
|---|---|---|
| **不在途**（如静态资源首次上传） | staging ring + `CopyBuffer` | 直接 map + memcpy（无 copy） |
| **在途**（GPU 正在读，如重上传/流式） | staging ring + `CopyBuffer` | staging ring + copy（或 fence 等待后直写） |

- **理由**：上一条「UMA = 直写」是错的——UMA 只是「目标可被 CPU 直接寻址」，但 CPU 写一块 GPU 正在读的内存仍是数据竞争。正确决策要同时看**拓扑**（能否直写）与**在途**（是否安全直写）。「在途」由 per-frame fence 判定。
- **备选**：无条件 staging（旧引擎做法）—— 被否，UMA 非在途时白多一次 copy；无条件直写（我上一条的错误）—— 被否，在途时数据竞争。

### D5: 在途判定依赖 frame fence

「目标是否在途」由 frame fence 判定（`aurora-frame-context` 的 `DeviceFrameContext`/`DeviceFrameDispatcher` 已持有 per-frame fence）。静态资源首次上传时目标尚未被任何 command 引用 → 不在途；重上传时目标被上一帧 command 引用 → 在途。

- **理由**：复用已有 frame 同步基础设施，不在上传层另造一套 fence。
- **备选**：调用方显式传 `isInFlight` 标志 —— 被否，把同步责任推给调用方，容易出错。

### D6: 两种上传模式（异步 upload queue vs 当帧 inline）

上传按「时序诉求」分两种，走两条不同路径：

| | **异步上传**（upload queue） | **当帧上传**（inline） |
|---|---|---|
| 语义 | 数据不要求当帧可用 | 数据必须在本帧绘制出来 |
| 队列 | transfer/upload 队列 | 当前帧图形/计算 command buffer |
| 同步 | `TransferTaskHandle` + `Wait`/`HasComplete`（fence） | command buffer 内联顺序保证 |
| staging 生命周期 | 持久 ring，fence 完成后回收 | 每帧 ring，帧末 `Reset()` |
| 典型用途 | 资源预加载、流式加载、后台导入 | 每帧动态数据（dynamic UBO、per-frame 顶点/索引、ImGui） |

- **异步路径**：`Queue::UploadBuffer(buffer, requests) -> TransferTaskHandle` **非阻塞**——把 staging→dst 的 copy 提交到 transfer 队列并挂 fence，返回句柄；调用方在真正消费数据前 `Wait(handle)` / 轮询 `HasComplete(handle)`。
- **当帧路径**：`StagingBufferAllocator`（每帧 bump 分配器，`Allocate(size, align)` 返回 `{buffer, offset, mapped}`），调用方写入后经 `BlitEncoder::CopyBuffer(staging, dst)` 内联记录在绘制命令之前，靠 command buffer 顺序保证可见，无跨队列同步。
- **理由**：把「何时必须可见」交给调用方，避免所有上传都走慢速的跨队列同步；当帧数据走内联 copy 零同步开销，预加载数据走后台 transfer 不阻塞帧。
- **备选**：只保留一条同步路径 —— 被否，无法兼顾「不阻塞帧」与「当帧可见」两种诉求。

## Risks / Trade-offs

- **[在途判定保守性]** 若无法精确判定目标是否被在途 command 引用，宁可走 staging（正确性优先，性能次之）。
- **[staging ring 大小]** 单帧上传量大时 ring 可能不够 → 缓解：v1 固定大小 + `Allocate` 失败则扩容/多段，扩容策略留后续。
- **[image 上传复杂度]** image 的 row/slice 对齐在各后端差异大 → 缓解：v1 只覆盖 `ImageUploadRequest` 已有字段的 buffer→image copy，复杂布局优化留后续。
- **[同步 vs 异步]** v1 同步提交 + 可等待句柄，避免异步队列的复杂性 → 缓解：接口返回句柄，后续可无缝换异步。

## Migration Plan

- 纯新增接口与后端实现，无现有调用方。
- 回滚：删除 `Queue` 上传方法与 staging ring 实现即可。

## Open Questions

- staging ring 的固定大小与扩容策略（v1 固定 + 失败扩容）。
- 「目标在途」判定的精确度：是保守（一律 staging）还是精确（按 lastWriter 判定）——v1 保守。
- `TransferTaskHandle` 是否需要（旧引擎有，新 aurora 尚无）——v1 引入，供调用方等待。
