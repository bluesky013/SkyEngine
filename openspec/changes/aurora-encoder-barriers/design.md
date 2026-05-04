## Context

Aurora `Core.h` 已经设计好两类 barrier 信息结构：

```cpp
struct ImageBarrierInfo {
    Image*        image = nullptr;
    ImageSubRange subRange;
    AccessFlags   srcAccess;
    AccessFlags   dstAccess;
};
struct BufferBarrierInfo { ... 类似 ... };
```

但接口缺：
- 这两个结构体没有 `oldLayout` / `newLayout` 字段；image layout transition 表达不出来
- 没有 `MemoryBarrierInfo`（用于 compute 之间的全局可见性）
- `Encoder::PipelineBarrier(...)` 方法不存在
- 没有把 `srcStage` / `dstStage` 作为 barrier-level 参数传入

`AccessFlagBit` 枚举设计得相当好（已按 stage × R/W × 资源类别细分），后端可以直接基于位掩码做转换。本 change 只需补齐 transition / stage / 接口三处。

约束：
- Vulkan 已强制 1.3，可用 sync2（`vkCmdPipelineBarrier2`），不必兼容老的 `vkCmdPipelineBarrier`
- Metal 大部分 barrier 由 encoder 边界隐式处理；只有同 encoder 内的 hazard 需要显式 `memoryBarrierWithScope:`
- DX12 用经典 `ResourceBarrier`，UAV 与 transition 是不同 type
- GLES 仅有 `glMemoryBarrier`，layout 概念不存在，转换为 noop 但 access mask 转 memory barrier bit

## Goals / Non-Goals

**Goals:**
- 三类 Encoder 都暴露 `PipelineBarrier(const BarrierInfo&)`
- 支持 image layout transition、buffer access barrier、global memory barrier
- 4 后端落地，Vulkan 与 DX12 跑通端到端 copy → render → readback 的 layout 链
- 提供 `InferLayoutForAccess(AccessFlags)` 减少调用方写 layout 的负担（最常见 access 一一对应一个标准 layout）

**Non-Goals:**
- 不做自动 barrier 推导（那是 RDG 的工作）
- 不支持 split barrier / queue family ownership transfer（mobile 优化项，未来 change）
- 不支持 sub-resource range 之外的更细粒度（如 VK_IMAGE_ASPECT_PLANE_*）
- 不在本 change 修复 `ImageBarrierInfo` 缺少 oldLayout/newLayout 之外的字段问题（如新增 `srcQueue`/`dstQueue`）

## Decisions

### 决策 1：PipelineBarrier 放 CommandBuffer，BarrierInfo 聚合传参

```cpp
struct MemoryBarrierInfo {
    AccessFlags srcAccess;
    AccessFlags dstAccess;
};

struct BarrierInfo {
    PipelineStageFlags                     srcStage;     // 可被 image/buffer-level 覆盖（仅 sync2 后端）
    PipelineStageFlags                     dstStage;
    std::vector<MemoryBarrierInfo>         memoryBarriers;
    std::vector<BufferBarrierInfo>         bufferBarriers;
    std::vector<ImageBarrierInfo>          imageBarriers;
};

class CommandBuffer { ...
    virtual void PipelineBarrier(const BarrierInfo &info) = 0;
};
```

**Why CommandBuffer 而非 Encoder：**
- Vulkan / DX12 / GLES 三家原生 API 都是 cmdbuf-级（`vkCmdPipelineBarrier2` / `ResourceBarrier` / `glMemoryBarrier`）
- Metal 是唯一 encoder-级，但 active-encoder 记账负担在哪都逃不掉——把它内部一次解决比让用户手动 EndEncoder→Barrier→BeginEncoder 干净
- Pass 之间 transition（Acquire→ColorAttachment、ColorAttachment→Present、ColorAttachment→ShaderRead）是最常见用法，cmdbuf-级 API 让用户写起来自然
- RDG 集成更直：在 pass 之间发 `cmdBuf->PipelineBarrier(...)` 不需要先创建空 encoder
- 三个 Encoder 各重复一份 PipelineBarrier 是噪音

**Why 聚合 BarrierInfo：** 调用方一次性提交一组 barrier 比多次 small call 高效（VK 强烈推荐合并），signatures 也更稳定。

**Alternatives considered:**
- *Encoder::PipelineBarrier*（本 change 早期方案）：被否决，理由如上
- *多个独立方法*（`ImageBarrier(...)` / `BufferBarrier(...)` / `MemoryBarrier(...)`）：调用更直观但合并不友好

**Metal 实现策略：**
```objc
void MetalCommandBuffer::PipelineBarrier(const BarrierInfo &info) {
    if (activeEncoder) {
        [activeEncoder memoryBarrierWithScope:scope after:srcStages before:dstStages];
    } else {
        pendingBarriers.push_back(info);    // flush at next CreateXxxEncoder
    }
}
```
Layout transition 在 Metal 上是 noop；缓存的 barrier 在下次 CreateGraphicsEncoder/CreateComputeEncoder/CreateBlitEncoder 入口处 flush。

### 决策 2：在 `ImageBarrierInfo` 上加 `oldLayout` / `newLayout`，但允许 UNDEFINED 让后端推导

```cpp
struct ImageBarrierInfo {
    Image*        image = nullptr;
    ImageSubRange subRange;
    AccessFlags   srcAccess;
    AccessFlags   dstAccess;
    ImageLayout   oldLayout = ImageLayout::UNDEFINED;     // UNDEFINED → 不关心旧内容
    ImageLayout   newLayout;                              // 必填
};
```

**Why:** `UNDEFINED` 旧 layout 是合法的"不关心旧内容"语义，对应 first-use 与 swapchain image 重置常见情形。

### 决策 3：`InferLayoutForAccess(AccessFlags) -> ImageLayout` 工具函数

接口层提供一个查表函数，最常见 access → layout 的一对一映射：

| AccessFlags 主项 | 推导 layout |
|---|---|
| `COLOR_WRITE` / `COLOR_INOUT_WRITE` | `COLOR_ATTACHMENT` |
| `DEPTH_STENCIL_WRITE` / `DEPTH_STENCIL_INOUT_WRITE` | `DEPTH_STENCIL_ATTACHMENT` |
| `DEPTH_STENCIL_READ` (只读) | `DEPTH_STENCIL_READ_ONLY` |
| `*_SRV`（VS/FS/CS/任何 stage 的 SRV） | `SHADER_READ_ONLY` |
| `*_UAV_*` | `GENERAL` |
| `TRANSFER_READ` | `TRANSFER_SRC` |
| `TRANSFER_WRITE` | `TRANSFER_DST` |
| `PRESENT` | `PRESENT` |
| 其它 / 多冲突 | `GENERAL` |

调用方仍可显式传 `oldLayout`/`newLayout`；推导仅作为便利，不强制。最常见路径上调用方只填 `srcAccess` / `dstAccess`，layout 自动得出。

**Why:** Vulkan layout 是负担最重的概念之一；很多 access flag 只对应一个 layout，重复表达浪费精力且容易出错。

### 决策 4：sync2 stage mask 来源——优先用 image/buffer-level 推导，fallback 用 BarrierInfo::srcStage/dstStage

`PipelineStageFlags` 是 barrier-level 参数。Vulkan sync2 允许每个 image/buffer barrier 各自带 stage，但本接口先简化为统一 stage（与 ResourceBarrier、MTLResourceBarrier 兼容）。**未来优化路径**：在 `ImageBarrierInfo` 加可选 stage 覆盖。

**Why:** DX12 ResourceBarrier 是 queue 级别同步，没有 per-resource stage 概念；统一 stage 是四后端最大公约数。

### 决策 5：4 后端的 barrier 翻译表

| Aurora 概念 | Vulkan | DX12 | Metal | GLES |
|---|---|---|---|---|
| Image layout transition | `VkImageMemoryBarrier2` | `D3D12_RESOURCE_BARRIER_TYPE_TRANSITION` | layout 不存在 → noop（`useResource:` 走 argument bind 时） | layout 不存在 → noop |
| Buffer access barrier | `VkBufferMemoryBarrier2` | `D3D12_RESOURCE_BARRIER_TYPE_UAV`（如 srcAccess 含 UAV_WRITE） | `memoryBarrierWithScope:` | `glMemoryBarrier(...)` |
| Memory barrier | `VkMemoryBarrier2` | UAV barrier (null resource = global) | `memoryBarrierWithScope:` | `glMemoryBarrier(GL_ALL_BARRIER_BITS)` |
| AccessFlags → access | 直接位转换 | 转 `D3D12_RESOURCE_STATES` | 转 `MTLRenderStages` + scope | 转 `glMemoryBarrier` bits |
| PipelineStage → stage | 直接位转换 | 忽略（DX12 是 queue-级） | render stages | 忽略 |

DX12 后端实现：从 srcAccess/dstAccess 推导 `D3D12_RESOURCE_STATES` 对（`COMMON` 是默认）；transition barrier 用资源 + 旧/新 state；memory barrier 转为 `D3D12_RESOURCE_BARRIER_TYPE_UAV`（null pResource 表示全局）。

### 决策 6：单点 API，不在 Encoder 上重复

由于决策 1 已把 PipelineBarrier 放到 CommandBuffer，Encoder 接口保持纯粹（仅 Encode 实际工作命令）。Blit 之前的 transition（如 SHADER_READ_ONLY → TRANSFER_DST）由调用方在 CreateBlitEncoder 之前 / 之后调 `cmdBuf->PipelineBarrier()` 完成；Metal 实现内部把这个 barrier 路由到正确的 encoder。

### 决策 7：Acquire / Present 关联的 PRESENT layout 由调用方显式 transition

后端不在 `Submit` / `AcquireNextImage` / `Present` 内部插隐式 barrier。调用方在录制 cmdbuf 时要显式：
- 渲染前：`UNDEFINED → COLOR_ATTACHMENT`（first-use 即可，VK swapchain image 不需要保留旧内容）
- 渲染后：`COLOR_ATTACHMENT → PRESENT`

**Why:** RDG 必然以"调用方显式 transition"为基础；隐式 transition 会与 RDG 自动 transition 冲突。文档化即可。

## Risks / Trade-offs

- **Metal 上 layout transition 是 noop 但 access mask 仍要落到 `memoryBarrierWithScope:`** → 缓解：在 Metal 后端单元测试中显式覆盖 compute → graphics 这条 hazard 路径
- **DX12 `D3D12_RESOURCE_STATES` 用位组合而非 enum** → 缓解：在 `D3D12Conversion` 加 `ToD3D12States(AccessFlags)`；维护一张 review 友好的对照表
- **GLES `glMemoryBarrier` 颗粒度低**（不区分 image / buffer / per-resource） → 缓解：GLES 后端把所有 barrier 拍成一次 `glMemoryBarrier`；由于 GLES 后端目标是功能正确性而非性能，可以接受
- **`InferLayoutForAccess` 对多冲突 access 返回 `GENERAL`** 会损失性能 → 缓解：在 Vulkan validation 上观察是否触发 `GENERAL` warning；高频路径鼓励调用方显式写 layout
- **修改 `ImageBarrierInfo` 是 schema-级改动** → 缓解：当前没有调用方使用，零迁移成本

## Migration Plan

无外部调用方。落仓顺序：
1. 修 `Core.h`（加 oldLayout/newLayout、加 `MemoryBarrierInfo`、加 `BarrierInfo`、加 `InferLayoutForAccess` 声明）
2. 加新 .cpp（接口层 `Barrier.cpp` 实现 `InferLayoutForAccess` + 验证用辅助函数）
3. Vulkan 后端落地 sync2 PipelineBarrier
4. DX12 / Metal / GLES 跟上
5. 测试一并合入

## Open Questions

- **是否要支持 split barrier?** VK sync2 / D3D12 enhanced barrier 都支持。倾向：本 change 不做；未来 change 加 `BeginSplitBarrier` / `EndSplitBarrier`。
- **Metal 缓存的 barrier 在 Submit 时还未 flush 怎么办？**（用户调 PipelineBarrier 后没创建 encoder 就 Submit）倾向：Submit 前 assert pendingBarriers 为空；调用方需保证 barrier 之后有实际 encoder 工作。
