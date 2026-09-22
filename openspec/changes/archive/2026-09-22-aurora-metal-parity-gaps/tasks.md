# Tasks: aurora-metal-parity-gaps

## 1. Blit / Resolve

- [x] 1.1 新增 `MetalBlitHelper`（fullscreen-triangle filtered blit，逐 format 缓存 pipeline；store-action resolve pass）
- [x] 1.2 `MetalBlitEncoder::BlitImage`：同格式等尺寸走原生 copy 快路径，否则 Suspend/Resume 包住 helper render pass
- [x] 1.3 `MetalBlitEncoder::ResolveImage`：Suspend/Resume 包住 resolve render pass
- [x] 1.4 `MetalDevice` 持有 `blitHelper`

## 2. 顶点输入与槽位分区

- [x] 2.1 `MetalUtils.h`：`METAL_VERTEX_BUFFER_SLOT_BASE` 常量 + `ToMetalVertexFormat`
- [x] 2.2 `MetalGraphicsPipeline`：从 `vertexBindings/vertexAttributes` 建 `MTLVertexDescriptor`；校验 shader buffer 槽预算
- [x] 2.3 `MetalShader`：暴露 `GetBufferSlotCount()`
- [x] 2.4 `BindVertexBuffers` 以 `METAL_VERTEX_BUFFER_SLOT_BASE` 偏移

## 3. ResourceGroup dynamic offsets / 数组

- [x] 3.1 `MetalResourceGroup`：reflection 收集 `*_DYNAMIC` binding（升序），`BindGraphics/BindCompute` 按序消费 dynamicOffsets
- [x] 3.2 `Write*` 增加 `arrayElement`（落槽 binding + arrayElement），encoder/batch 透传
- [x] 3.3 图形/计算 encoder `BindResourceGroup` 透传 dynamicOffsets

## 4. Push constant partial-range

- [x] 4.1 图形/计算 encoder CPU staging block 累积 + 整块 `set*Bytes`
- [x] 4.2 `MetalShader` 从 `reflection.pushConstants` 推导 `pushConstantSize`

## 5. SwapChain / CommandPool

- [x] 5.1 `MetalSwapChain`：3-image slot ring，`maximumDrawableCount=3`，`framebufferOnly=NO`，`Present(imageIndex)` 按 slot 释放
- [x] 5.2 `MetalCommandPool`：freeList 复用，`Reset()` 回收

## 6. Shader 加载 / encoder 语义 / 格式 / caps

- [x] 6.1 `MetalShaderFunction`：`MTLB` magic 走 `newLibraryWithData`，否则 MSL 源码编译
- [x] 6.2 `BeginRendering` 用 `renderArea` 播种 viewport+scissor
- [x] 6.3 `Draw*Indirect`：`stride==0` 按紧凑排列；多 viewport/scissor 告警取 0
- [x] 6.4 `ToMetalPixelFormat` 补 BC1-7 / ASTC 映射
- [x] 6.5 `UpdateDeviceCaps`：meshShader / framebufferFetch / firstInstanceIndirect / descriptorHeap=false / descriptorIndexing=false

## 7. 文档与 spec

- [x] 7.1 `engine/aurora/AGENTS.md` Metal 小节同步
- [x] 7.2 `openspec/specs/aurora-rhi-metal` 主 spec 同步
