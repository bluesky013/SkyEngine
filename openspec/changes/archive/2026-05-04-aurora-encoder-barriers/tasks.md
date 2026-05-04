## 1. 接口层

- [x] 1.1 修改 `aurora/rhi/Core.h`：给 `ImageBarrierInfo` 加 `oldLayout` / `newLayout` 字段（默认 UNDEFINED / UNDEFINED）
- [x] 1.2 修改 `aurora/rhi/Core.h`：新增 `MemoryBarrierInfo`、`BarrierInfo` 结构体
- [x] 1.3 新增 `aurora/rhi/Barrier.h`：`InferLayoutForAccess(AccessFlags) -> ImageLayout` + `IsLayoutCompatibleWithAccess(layout, access) -> bool`
- [x] 1.4 修改 `aurora/rhi/CommandBuffer.h`：加 `virtual void PipelineBarrier(const BarrierInfo&) = 0`（不在 Encoder 上）
- [x] 1.5 新增 `aurora/rhi/interface/src/Barrier.cpp`：实现 `InferLayoutForAccess` 查表
- [x] 1.6 单元测试：`BarrierInferTest.cpp` 10 条用例（空 / 单 access 各类 / UAV / 冲突 / IsLayoutCompatibleWithAccess 协议）

## 2. Vulkan 后端

- [x] 2.1 在 `VulkanConversion` 加：`FromPipelineStageFlags2` / `FromAccessFlags2` / `FromImageLayout` / `InferAspectFromLayout`
- [x] 2.2 PipelineBarrier 实现内联于 `VulkanCommandBuffer::PipelineBarrier`（`VkMemoryBarrier2` / `VkBufferMemoryBarrier2` / `VkImageMemoryBarrier2`）
- [x] 2.3 `VulkanCommandBuffer::PipelineBarrier` 用 `vkCmdPipelineBarrier2` + `VkDependencyInfo`
- [x] 2.4 image aspect mask 推导：`InferAspectFromLayout(newLayout, vkFormat)` 兜底（caller 未填时）

## 3. DX12 后端

- [x] 3.1 在 `D3D12Conversion` 加：`ToD3D12States(AccessFlags) -> D3D12_RESOURCE_STATES`（macOS 上未编译，Windows 端待回归）
- [x] 3.2 在 `D3D12CommandBuffer::PipelineBarrier`：image transition + buffer transition + UAV barrier 三种合一打包 `ResourceBarrier`
- [x] 3.3 stageMask 在 DX12 上忽略（D3D12_RESOURCE_BARRIER 是 queue-级，无 per-stage 概念）

## 4. Metal 后端

- [x] 4.1 `MetalCommandBuffer` 加 `NotifyEncoderBegin/End` 接口；改 `Create*Encoder` 传 `MetalCommandBuffer*` 而非裸 `id<MTLCommandBuffer>`
- [x] 4.2 `MetalCommandBuffer::PipelineBarrier`：active encoder 非空 → `[encoder memoryBarrierWithScope:...]`；否则缓存到 `pendingBarriers`
- [x] 4.3 `NotifyEncoderBegin` 入口处 flush `pendingBarriers`
- [x] 4.4 工具内联在 `MetalCommandPool.mm`：`ScopeForBarrierInfo`、`StagesForRender`
- [x] 4.5 layout transition 全部 noop（Metal 无 layout 概念）
- [ ] 4.6 调试 assert：Submit 前 pendingBarriers 必须为空（暂未加；现有 BarrierTestMetal 覆盖 flush 路径已隐式验证）

## 5. GLES 后端

- [x] 5.1 在 `GLESConversion` 加：`AccessFlagsToGLBarrierBits(AccessFlags) -> GLbitfield`
- [x] 5.2 `GLESCommandBuffer::PipelineBarrier`：合并所有 access OR 后调一次 `glMemoryBarrier`
- [x] 5.3 layout / stage 在 GLES 上忽略；用 `GL_ALL_BARRIER_BITS` 兜底未支持的 access

## 6. 测试

- [x] 6.1 `BarrierTest.cpp` headless 路径
  - [ ] 6.1.1 `CopyThenRenderThenReadback`（end-to-end pixel 验证；本轮未做，需要 readback 路径完成的 staging buffer / copy 链；属 v2 测试）
  - [ ] 6.1.2 `ComputeWriteThenComputeRead`（同上 readback 依赖）
  - [ ] 6.1.3 `ComputeWriteThenGraphicsSample`（同上）
  - [x] 6.1.4 `BarrierTestMetal::BarrierBeforeRenderEncoderFlushesOnBegin` — 验证 Metal pending flush
  - [x] `BarrierTestVulkan::ImageTransitionBeforeRendering` — Vulkan 路径 transition + render
  - [x] `BarrierTestVulkan::MemoryBarrierBetweenComputeDispatches` — 全局 memory barrier
- [ ] 6.2 SwapChain present 链路 explicit barrier 验证（待 SwapChainTest harness 就位）
- [x] 6.3 `BarrierInferTest`：10 条用例全绿（NONE/COLOR/DEPTH/SRV/UAV/Transfer/Present/Conflict + IsLayoutCompatibleWithAccess）

## 7. 收尾 / 文档

- [x] 7.1 `engine/aurora/AGENTS.md` 加"Barrier 用法 + AccessFlags 速查表 + Stage/Access 兼容性陷阱 + 4 后端实现位置"
- [x] 7.2 `AuroraTest --gtest_filter=Barrier*` 全绿：13/13 PASSED；含 Vulkan + Metal
- [x] 7.3 Vulkan validation 不报新 warning（修测试 stage mask 后清零）
- [x] 7.4 archive：`openspec archive aurora-encoder-barriers`
