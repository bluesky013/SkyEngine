# Design: aurora-metal-parity-gaps

## 槽位分区（buffer slot partition）

Metal 每 stage 只有 31 个 buffer argument 槽（0..30）。slang MSL 把 shader buffer 从 0 起连续编号（push constant 占最高使用槽），顶点缓冲若也从 0 起绑必然冲突。定案：

- shader buffer（含 push constant）：`[0, METAL_VERTEX_BUFFER_SLOT_BASE)`
- 顶点缓冲 binding i：`METAL_VERTEX_BUFFER_SLOT_BASE + i`
- `METAL_VERTEX_BUFFER_SLOT_BASE = 31 - MAX_VERTEX_BINDINGS = 15`

`MTLVertexDescriptor` 的 `layouts[]`/`attributes[].bufferIndex` 与 `BindVertexBuffers` 使用同一基准；pipeline 创建时 `SKY_ASSERT` + 报错校验 `shader->GetBufferSlotCount() <= 15`。备选方案（argument buffer 统一装顶点+资源）被否：tier-1 直绑模型下无 argument buffer，且改动面大。

## Blit/Resolve 双路径

MTLBlitCommandEncoder 只能 1:1 copy。策略：

- **快路径**：同 pixelFormat 且所有 region 等尺寸 → 原生 `copyFromTexture`，按 dst layers 循环（src layers>1 时逐层对应）；1:1 拷贝下 filter 无意义，不参与路径选择。
- **慢路径**：`MetalBlitHelper::Blit` 用懒构建的 fullscreen-triangle pipeline（按 `(dst pixelFormat, sampleCount)` 缓存 `MTLRenderPipelineState`），逐 region 逐 layer 一个 render pass：src 经单 level/单 slice view 采样，dst rect 同时作 viewport 与 scissor；pipeline 或 render encoder 创建失败返回 false。
- **Resolve**：Metal 无显式 resolve blit，用 `MTLStoreActionMultisampleResolve` 的空 render pass（src 作 attachment、loadAction=Load，dst 作 resolveTexture，endEncoding 时执行 resolve）。

`MetalBlitEncoder` 持有一个活跃 `MTLBlitCommandEncoder`；进入 render pass 前 `SuspendBlit()`（endEncoding + NotifyEncoderEnd），结束后 `ResumeBlit()` 新建 blit encoder。helper 由 `MetalDevice` 持有以共享 pipeline 缓存。

## dynamic offsets 消费顺序

Metal 直绑没有 Vulkan descriptor 的 dynamic-offset 数组语义，对齐 DX12：reflection 中 `*_DYNAMIC` binding 升序收集进 `dynamicBindings`，bind 时把 buffer 表排序遍历，命中即按序消费 `dynamicOffsets[i]` 加到记录 offset 上（稳定 descriptor + per-draw offset）。缺 offset 时告警并回退记录值。

## SwapChain ring

CAMetalLayer 内部 drawable 池（`maximumDrawableCount=3`）。`AcquireNextImage` 轮转 3 个 slot：释放该 slot 旧 drawable → `nextDrawable` → 重建 borrowed `MetalImage`；`Present(imageIndex)` 提交 present 后释放该 slot。acquire 仍 CPU 立即 signal（Metal 无 GPU acquire 语义）。`framebufferOnly=NO` 让 backbuffer 可被 RDG 采样/拷贝。

## push constant staging

`set*Bytes` 无基偏移，partial push 无法直接表达。encoder 持 CPU 侧 `pushConstantBlock`（按 reflection 块大小/最大 offset+size 增长），每次 `PushConstants(offset,size,data)` memcpy 进 block 后整块重传。开销为 O(块大小)，块通常 < 256B，可接受。
