## Why

Aurora `Core.h` 已经定义了 `ImageBarrierInfo`、`BufferBarrierInfo`、`AccessFlagBit`（按 stage × 读写 × 资源类别细分）和 `PipelineStageBit`，**但没有任何 Encoder 方法接受它们**。意味着：跨 pass 复用资源、Acquire 后的 swapchain image layout 转换、CopyBufferToImage 前后的 layout 切换，目前都做不到——aurora 现状就是只能跑单 pass 的 clear-screen。Barrier API 是接通 SwapChain 渲染、ResourceGroup 绑定、RDG 自动屏障推导的前置 P0 项。

- 在 **`CommandBuffer`** 上加 **`PipelineBarrier(const BarrierInfo&)`** 方法，接受一组 image + buffer + memory barrier。**不**放在 Encoder 上——native API 中 3/4 后端是 cmdbuf-级，Metal 是唯一 encoder-级（由 RHI 内部记账 active encoder 隐藏）
- 新增 `MemoryBarrierInfo`（仅 srcAccess / dstAccess，全局可见性）补齐 `ImageBarrierInfo` / `BufferBarrierInfo`
- 在 `ImageBarrierInfo` 上加 `oldLayout` / `newLayout`（当前结构没这俩字段，无法表达 layout transition）
- 在 `BarrierInfo` 上加聚合的 `srcStage` / `dstStage`（PipelineStageFlags），后端按需用 sync2 或扁平化
- **AccessFlags ↔ Layout 自动推导工具**：在接口层提供 `InferLayoutForAccess(AccessFlags)`，在最常见路径下让调用方不必显式写 layout
- 4 个后端落地：
  - Vulkan：`vkCmdPipelineBarrier2`（sync2，VK 1.3 已强制）直接落到 cmdbuf
  - DX12：`ResourceBarrier` 落到 cmdlist；UAV barrier 用 null pResource
  - Metal：CommandBuffer 内部记账 active encoder：调用时若有活跃 encoder，转 `[encoder memoryBarrierWithScope:after:before:]`；若无活跃 encoder，缓存到下一个 CreateXxxEncoder 时 flush
  - GLES：`glMemoryBarrier(...)`；layout / stage 忽略
- 新增 `BarrierTest.cpp`：copy → render → readback；layout transition 验证；compute → graphics R/W hazard

## Capabilities

### New Capabilities
- `aurora-barriers`: Encoder 上的 pipeline barrier 接口（image layout + access mask + stage mask + memory）

### Modified Capabilities
（无；Encoder 接口由 aurora-queue-submit-present 之外的本 change 独立扩展）

## Impact

- **接口头**：新增 `aurora/rhi/Barrier.h`；修改 `aurora/rhi/Core.h`（给 `ImageBarrierInfo` 加 oldLayout/newLayout）；修改 `aurora/rhi/Encoder.h`（三类 encoder 加 `PipelineBarrier`）
- **后端**：Vulkan / DX12 / Metal / GLES 各加 barrier 实现；Vulkan 的 `VulkanConversion` 加 AccessFlags / PipelineStage 转换工具
- **依赖**：本 change 与 `aurora-queue-submit-present` 在接口层解耦；可并行实现，但端到端测试（render → copy → readback）需要 Submit 已就绪
- **调用方**：`SwapChain::AcquireNextImage` 之后从 PRESENT → COLOR_ATTACHMENT 的 layout 转换将依赖此 API
