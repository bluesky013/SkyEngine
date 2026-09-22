# Change: aurora-metal-parity-gaps

## Why

`aurora-metal-blocking-gaps` 之后 Metal 后端可编译、可绑定，但对照 Vulkan/DX12 仍有一组功能级缺口，阻塞实际渲染与跨后端对齐：

1. `BlitImage`/`ResolveImage` 是空壳：MTLBlitCommandEncoder 不支持缩放/过滤 blit 与显式 MSAA resolve。
2. 顶点输入未接入：无 `MTLVertexDescriptor`，shader buffer 绑定与顶点缓冲槽位会互相踩踏（Metal 每 stage 仅 31 个 `[[buffer(N)]]` 槽）。
3. `*_DYNAMIC` buffer 的 dynamic offsets 被直接忽略（只告警），descriptor 数组的 `arrayElement` 被丢弃。
4. PushConstants 不支持 partial-range（`set*Bytes` 无基偏移）。
5. SwapChain 单 image（`GetImageCount()==1`），acquire 即释放上一 drawable，`framebufferOnly=YES` 使 backbuffer 不可采样，与三后端 in-flight ring 语义不一致。
6. `MetalCommandPool::Reset()` 空实现，每帧分配泄漏式累积命令缓冲包装。
7. shader 只接受 MSL 源码文本，不支持预编译 `.metallib` blob。
8. `renderArea` 不播种 viewport/scissor（DX12 契约）、indirect draw 忽略 stride、BC/ASTC 压缩格式与顶点 attribute `Format` 无映射、device feature caps（meshShader/framebufferFetch/descriptorHeap 等）未填。

## What Changes

- **Blit/Resolve**：新增 `MetalBlitHelper`（`MetalDevice` 持有）。同格式且所有 region 等尺寸的 blit 走 blit encoder 原生 `copyFromTexture`（按 dst layer 循环，filter 不参与路径选择）；格式不同或尺寸不同走懒构建 fullscreen-triangle render pass（pipeline 按 `(dst format, sampleCount)` 缓存，逐 region 逐 layer 一个 pass，dst 不可渲染时返回 false 并报错）；resolve 走 `MTLStoreActionMultisampleResolve` 空 render pass。`MetalBlitEncoder` 在 render pass 前后挂起/恢复原生 blit encoder。
- **顶点输入与槽位分区**：`MetalUtils.h` 定义 `METAL_VERTEX_BUFFER_SLOT_BASE = 31 - MAX_VERTEX_BINDINGS`（15）；shader buffer（含 push constant 最高槽）占 `[0,15)`，顶点缓冲占 `[15,31)`。`MetalGraphicsPipeline` 从 `vertexBindings/vertexAttributes` 建 `MTLVertexDescriptor`，并校验 shader buffer 槽数不超预算；`BindVertexBuffers` 同步偏移。
- **dynamic offsets + 数组**：`MetalResourceGroup` 从 shader reflection 收集 `*_DYNAMIC` binding（升序），bind 时按序消费 `dynamicOffsets`（与 DX12 root CBV/UAV 契约一致）；`Write*` 增加 `arrayElement`，落槽 `binding + arrayElement`（slang MSL 数组摊平为连续 argument index）。
- **push constant partial**：encoder 侧 CPU staging block 累积 `(offset,size)` 写入后整块 `set*Bytes` 重传；`MetalShader` 从 `reflection.pushConstants` 推导块大小。
- **SwapChain ring**：`IMAGE_COUNT=3`，`maximumDrawableCount=3`，acquire 轮转 slot、`Present(imageIndex)` 释放对应 slot；`framebufferOnly=NO` 允许 backbuffer 被采样/拷贝。
- **命令缓冲复用**：`MetalCommandPool` 增加 freeList，`Reset()` 回收全部已分配包装（`Begin()` 重建原生 MTLCommandBuffer），`Allocate()` 优先复用。
- **metallib 加载**：binary 以 `MTLB` magic 开头走 `newLibraryWithData`，否则按 MSL 源码编译。
- **encoder 语义对齐**：`BeginRendering` 用 `renderArea` 播种 viewport+scissor；`Draw*Indirect` 的 `stride==0` 解释为紧凑排列（同 Vulkan）；单 viewport/scissor 超出时告警取 0。
- **格式与 caps**：`ToMetalPixelFormat` 补 BC1-7（桌面 GPU）与 ASTC（Apple GPU）映射（ETC2 无对应）；新增 `ToMetalVertexFormat`；`UpdateDeviceCaps` 填 `meshShader`（Apple7/Mac2）、`framebufferFetch`（Apple1）、`firstInstanceIndirect=true`、`descriptorHeap/descriptorIndexing=false`（slang MSL 不产 argument buffer）。

## Capabilities

### Modified Capabilities

- `aurora-rhi-metal`: ResourceGroup 直接绑定模型补 dynamic offsets 与数组元素；push constant 约定补 partial-range；新增 blit/resolve、顶点输入槽位分区、swapchain ring、shader 库加载、命令缓冲池复用、格式映射与 device caps 要求。

## Impact

- `engine/aurora/rhi/metal/**`：encoder / resource group / shader / pipeline / swapchain / command pool / device / utils，新增 `MetalBlitHelper.h/.mm`
- `engine/aurora/AGENTS.md`：Metal 小节描述同步
- 无 RHI 核心接口变更；Vulkan/DX12 后端不受影响
