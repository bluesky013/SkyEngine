## 1. 接口层

- [ ] 1.1 修改 `aurora/rhi/Core.h`：给 `ImageBarrierInfo` 加 `oldLayout` / `newLayout` 字段（默认 UNDEFINED / 必填）
- [ ] 1.2 修改 `aurora/rhi/Core.h`：新增 `MemoryBarrierInfo`、`BarrierInfo` 结构体
- [ ] 1.3 新增 `aurora/rhi/Barrier.h`：`InferLayoutForAccess(AccessFlags) -> ImageLayout` 声明 + `IsLayoutCompatibleWithAccess(layout, access) -> bool` 调试辅助
- [ ] 1.4 修改 `aurora/rhi/Encoder.h`：`GraphicsEncoder` / `ComputeEncoder` / `BlitEncoder` 各加 `virtual void PipelineBarrier(const BarrierInfo&) = 0`
- [ ] 1.5 新增 `aurora/rhi/interface/src/Barrier.cpp`：实现 `InferLayoutForAccess` 查表；按 spec 中 7 条规则
- [ ] 1.6 单元测试：`BarrierInferTest.cpp`（不依赖 device，仅测 `InferLayoutForAccess` 的所有规则）

## 2. Vulkan 后端

- [ ] 2.1 在 `VulkanConversion` 加：`FromAccessFlags2(AccessFlags) -> VkAccessFlags2` / `FromPipelineStage2(PipelineStageFlags) -> VkPipelineStageFlags2` / `FromImageLayout(ImageLayout) -> VkImageLayout`
- [ ] 2.2 加 `FromImageBarrier(ImageBarrierInfo, srcStage, dstStage) -> VkImageMemoryBarrier2` 等三个 barrier 转换工具
- [ ] 2.3 在 `VulkanGraphicsEncoder::PipelineBarrier` 用 `vkCmdPipelineBarrier2` + `VkDependencyInfo`
- [ ] 2.4 在 `VulkanComputeEncoder::PipelineBarrier` / `VulkanBlitEncoder::PipelineBarrier` 同样实现
- [ ] 2.5 处理 image aspect mask 推导：复用 `InferAspectMask`（已有）

## 3. DX12 后端

- [ ] 3.1 在 `D3D12Conversion` 加：`ToD3D12States(AccessFlags) -> D3D12_RESOURCE_STATES`
- [ ] 3.2 在 `D3D12GraphicsEncoder::PipelineBarrier`：把 `imageBarriers` 翻成 `D3D12_RESOURCE_BARRIER_TYPE_TRANSITION`；`memoryBarriers` 中含 UAV write 的翻成 `D3D12_RESOURCE_BARRIER_TYPE_UAV`（pResource = null 即全局）；批量调用 `ResourceBarrier`
- [ ] 3.3 ComputeEncoder / BlitEncoder 同样；BlitEncoder 上的 transition 必须落到对应 cmdlist
- [ ] 3.4 stageMask 在 DX12 上忽略；文档化在 `D3D12Encoder.h` 注释里

## 4. Metal 后端

- [ ] 4.1 在 `MetalEncoder` 三类 encoder 上实现 `PipelineBarrier`：layout transition 全部 noop；memoryBarrier / bufferBarrier 转为 `[encoder memoryBarrierWithScope:after:before:]`
- [ ] 4.2 加 `MetalUtils.h` 工具：`AccessToScope(AccessFlags) -> MTLBarrierScope`、`AccessToStages(AccessFlags) -> MTLRenderStages`

## 5. GLES 后端

- [ ] 5.1 在 `GLESConversion` 加：`AccessFlagsToGLBarrierBits(AccessFlags) -> GLbitfield`
- [ ] 5.2 在 `GLESGraphicsEncoder::PipelineBarrier`（+ Compute/Blit）：把全部 barrier 的 access 合并 OR 后调用一次 `glMemoryBarrier`；layout / stage 全部忽略
- [ ] 5.3 处理 OpenGL ES 不支持 `glTextureBarrier` 时的 fallback（直接 `glMemoryBarrier(GL_ALL_BARRIER_BITS)`）

## 6. 测试

- [ ] 6.1 `BarrierTest.cpp` headless 路径（依赖 `aurora-queue-submit-present` 的 Submit 已可用；如未合入，先 mock）
  - [ ] 6.1.1 `CopyThenRenderThenReadback`：upload buffer → CopyBufferToImage → barrier(TRANSFER_DST→COLOR_ATTACHMENT) → BeginRendering 用 LoadOp::LOAD → Draw → EndRendering → barrier(COLOR_ATTACHMENT→TRANSFER_SRC) → CopyImageToBuffer → readback 验证像素
  - [ ] 6.1.2 `ComputeWriteThenComputeRead`：Dispatch 写 UAV → memoryBarrier → Dispatch 读 UAV → readback 验证
  - [ ] 6.1.3 `ComputeWriteThenGraphicsSample`：Dispatch UAV → barrier(UAV_WRITE→FRAGMENT_SRV, GENERAL→SHADER_READ_ONLY) → 采样并渲染 → readback
- [ ] 6.2 验证 SwapChain present 链路：`SwapChainTest::AcquireRenderPresentLoop`（在 `aurora-queue-submit-present` 中已有）补 explicit barrier；validation layer 不报警
- [ ] 6.3 `BarrierInferTest`：枚举所有规则；冲突 access 退化 GENERAL；空 access 返回 UNDEFINED

## 7. 收尾 / 文档

- [ ] 7.1 在 `engine/aurora/AGENTS.md`（aurora-queue-submit-present 已起的）补一节"Barrier 用法 + AccessFlags 速查表"，含 InferLayoutForAccess 的 7 条规则
- [ ] 7.2 跑 `AuroraTest --gtest_filter=*Barrier*` 全绿（至少 Vulkan + 当前 host 平台第二后端）
- [ ] 7.3 Vulkan validation / D3D12 debug layer 不报新 warning
- [ ] 7.4 archive：`openspec archive aurora-encoder-barriers`
