# Delta: aurora-rhi-metal

## MODIFIED Requirements

### Requirement: Metal ResourceGroup 直接绑定模型

slang Metal target SHALL 被视为扁平绑定模型：资源按类别（buffer/texture/sampler）各自连续编号（`[[buffer(N)]]`/`[[texture(N)]]`/`[[sampler(N)]]`)，`[[vk::binding]]` 的 space 被忽略，MSL 反射的 `binding` 即类别内索引。Metal ResourceGroup SHALL 以 binding→资源表记录写入，并在 `BindResourceGroup` 时以 `setBuffer/setTexture/setSampler` 直接应用（无 argument buffer)。`DescriptorEncoder`/`DescriptorBatch` SHALL 立即写入 group 表（无批量原生对象）。`CreateResourceGroup`/`CreateDescriptorBatch` SHALL NOT 返回 nullptr。

descriptor 数组：slang MSL 将资源数组摊平为从数组基址起连续的 argument index；`Write*` 的 `arrayElement` SHALL 落槽 `binding + arrayElement`。

动态 buffer：`*_DYNAMIC` 类型的 binding SHALL 从 shader reflection 派生并按升序记录；bind 时 SHALL 按该顺序消费 `dynamicOffsets` 并加到记录 offset 上（稳定 descriptor + per-draw offset，与 DX12 root CBV/UAV 契约一致）；dynamicOffsets 不足时 SHALL 告警并回退记录 offset。

#### Scenario: ResourceGroup 创建与写入

- **WHEN** pipeline 层以 `{shader, set}` 创建 ResourceGroup 并经 encoder/batch 写入 buffer/image/sampler
- **THEN** 对象创建成功，写入记录进绑定表（`set` 与 `ImageLayout` 在 Metal 上忽略）

#### Scenario: 绑定应用

- **WHEN** 图形/计算 encoder `BindResourceGroup`
- **THEN** group 内所有 buffer/texture/sampler 被设置到对应类别的 binding 槽位（图形同时设置 vertex 与 fragment 阶段）

#### Scenario: descriptor 数组元素写入

- **WHEN** 对数组 binding 以 `arrayElement=2` 写入 texture
- **THEN** 该 texture 落槽 `binding + 2`

#### Scenario: 动态 offset 应用

- **WHEN** group 含两个 `UNIFORM_BUFFER_DYNAMIC` binding（升序 b1<b2）且 `BindResourceGroup` 传入 `[o1, o2]`
- **THEN** b1 以 `记录offset+o1`、b2 以 `记录offset+o2` 绑定

### Requirement: Metal push constant 槽位约定

slang 在 Metal 上把 `[[vk::push_constant]]` 物化为普通 `constant T* [[buffer(N)]]`。引擎约定 push constant 块在 shader 中最后声明；`MetalShader` SHALL 将 push constant 槽位推导为 buffer 类资源的最高 binding + 0（即资源数 - 1），并 SHALL 从 `reflection.pushConstants` 推导块大小（max offset+size）。encoder `PushConstants` SHALL 用 `setVertexBytes/setFragmentBytes/setBytes`（按 stageFlags）写入该槽位。

`set*Bytes` 无基偏移；partial-range push SHALL 先在 CPU 侧 staging block 按 `(offset,size)` 累积，再整块重传该槽位。

#### Scenario: push constant 写入

- **WHEN** shader 声明 push constant 块（最后声明）且 encoder 调 `PushConstants`
- **THEN** 数据经 `set*Bytes` 写入推导槽位，不与普通 buffer 资源冲突

#### Scenario: partial-range push

- **WHEN** 对同一块先后以 `(offset=0,size=16)` 与 `(offset=16,size=16)` 调 `PushConstants`
- **THEN** 两次写入都落在 staging block 对应偏移，最终整块 32 字节写入槽位

## ADDED Requirements

### Requirement: Metal blit 与 resolve

同 pixelFormat 且所有 region src/dst 等尺寸的 `BlitImage` SHALL 走 MTLBlitCommandEncoder 原生 `copyFromTexture`（按 dst layer 循环；1:1 texel 拷贝下 filter 无意义，不影响路径选择）。格式不同或任一 region 尺寸不同的 blit SHALL 经 `MetalBlitHelper` 的 fullscreen-triangle render pass 完成（pipeline 按 `(dst pixelFormat, sampleCount)` 懒构建并缓存，逐 region 逐 layer 一个 pass，以 dst rect 设置 viewport/scissor）；dst 格式不可渲染（pipeline 或 render encoder 创建失败）时 SHALL 返回失败并记录错误。`ResolveImage` SHALL 经 `MTLStoreActionMultisampleResolve` 的空 render pass 完成（src 作 attachment、dst 作 resolveTexture，逐 region 逐 layer 一个 pass）。进入 render pass 前 `MetalBlitEncoder` SHALL 挂起当前 blit encoder，结束后恢复新的 blit encoder。

#### Scenario: 1:1 copy 快路径

- **WHEN** src/dst 同格式且各 region src/dst 尺寸相同
- **THEN** 使用 blit encoder `copyFromTexture`，不创建 render pass

#### Scenario: 缩放/转格式 blit

- **WHEN** src/dst pixelFormat 不同，或任一 region 的 src/dst 尺寸不同
- **THEN** blit encoder 挂起，`MetalBlitHelper` 以 fullscreen-triangle pass 采样 src（filter 选择 linear/nearest sampler）写入 dst，随后恢复 blit encoder

#### Scenario: MSAA resolve

- **WHEN** 对 MSAA src 调 `ResolveImage`
- **THEN** 通过 store-action resolve render pass 写入单采样 dst

### Requirement: Metal 顶点输入与 buffer 槽位分区

Metal 每 stage 31 个 buffer 槽 SHALL 分区：shader buffer（含 push constant 最高槽）占 `[0, METAL_VERTEX_BUFFER_SLOT_BASE)`，顶点缓冲 binding i 占 `METAL_VERTEX_BUFFER_SLOT_BASE + i`，其中 `METAL_VERTEX_BUFFER_SLOT_BASE = 31 - MAX_VERTEX_BINDINGS`。`MetalGraphicsPipeline` SHALL 从 `PipelineState::vertexBindings/vertexAttributes` 构建 `MTLVertexDescriptor`（layout/attribute 的 bufferIndex 使用同一基准），并 SHALL 在 shader buffer 槽数超过 `METAL_VERTEX_BUFFER_SLOT_BASE` 时断言并使创建失败。

#### Scenario: 顶点输入生效

- **WHEN** pipeline state 声明 vertex binding/attribute 并绑定顶点缓冲后 Draw
- **THEN** vertex descriptor 将 attribute 映射到 `METAL_VERTEX_BUFFER_SLOT_BASE + binding`，顶点数据正确进入 stage_in

#### Scenario: 槽位超预算

- **WHEN** shader 的 buffer 类资源槽数超过 `METAL_VERTEX_BUFFER_SLOT_BASE`
- **THEN** pipeline 创建断言并返回失败

### Requirement: Metal SwapChain 多 image ring

`MetalSwapChain` SHALL 以 `IMAGE_COUNT=3` 个 slot 轮转 acquire（`CAMetalLayer.maximumDrawableCount=3`）：`AcquireNextImage` 返回轮转 index 并保持已 acquire 的 drawable 有效直到对应 `Present(imageIndex)`。`framebufferOnly` SHALL 为 `NO` 以允许 backbuffer 被采样/拷贝。acquire 的 signal 语义 SHALL 保持 CPU 立即 signal（Metal 无 GPU acquire 钩子）。

#### Scenario: 轮转 acquire

- **WHEN** 连续 acquire 三次未 present
- **THEN** 返回 index 依次为 0、1、2，三个 backbuffer image 同时有效

#### Scenario: present 释放 slot

- **WHEN** `Present(imageIndex=1)` 完成
- **THEN** slot 1 的 drawable 被释放，下次轮转到 1 时重新 acquire

### Requirement: Metal shader 库加载（源码与 metallib）

`MetalShaderFunction` SHALL 按 binary 内容选择加载路径：以 `MTLB` magic 开头的预编译 `.metallib` blob 走 `newLibraryWithData`；其余按 MSL 源码文本走 `newLibraryWithSource`。

#### Scenario: metallib blob 加载

- **WHEN** shader binary 为离线编译的 `.metallib`
- **THEN** 经 `newLibraryWithData` 创建 library 成功

### Requirement: Metal 命令缓冲池复用

`MetalCommandPool::Reset()` SHALL 将全部已分配 `MetalCommandBuffer` 回收至 freeList（包装复用，原生 MTLCommandBuffer 由 `Begin()` 重建）；`Allocate()` SHALL 优先从 freeList 取用。

#### Scenario: 池复用

- **WHEN** 分配若干命令缓冲后 `Reset()` 再 `Allocate()`
- **THEN** 返回此前分配的包装对象，不新增分配

### Requirement: Metal encoder 语义对齐

`BeginRendering` SHALL 在 `renderArea` 非空时以其播种初始 viewport 与 scissor（对齐 DX12 契约）。`DrawIndirect`/`DrawIndexedIndirect` 的 `stride==0` SHALL 解释为参数紧凑排列（同 Vulkan），非零 stride SHALL 不小于对应 `MTLDraw*IndirectArguments` 大小。`SetViewport`/`SetScissor` 收到多于 1 个矩形时 SHALL 告警并使用第 0 个（Metal 单 viewport/scissor）。

#### Scenario: renderArea 播种

- **WHEN** `BeginRendering` 携带非零 `renderArea`
- **THEN** encoder 初始 viewport/scissor 等于该区域

#### Scenario: 紧凑 indirect 参数

- **WHEN** `DrawIndirect` 以 `stride=0`、`drawCount=4` 调用
- **THEN** 四次 draw 分别以 `offset + i * sizeof(MTLDrawPrimitivesIndirectArguments)` 读取参数

### Requirement: Metal 像素/顶点格式映射与 device caps

`ToMetalPixelFormat` SHALL 覆盖 BC1-7（桌面 GPU）与 ASTC LDR（Apple GPU）压缩格式；ETC2 无 Metal 对应，返回 Invalid。`ToMetalVertexFormat` SHALL 覆盖 `Format` 的 float/unorm/uint 变体。`UpdateDeviceCaps` SHALL 填充：`meshShader`（Apple7+/Mac2+）、`framebufferFetch`（Apple1+）、`firstInstanceIndirect=true`、`descriptorHeap=false`、`descriptorIndexing=false`（slang MSL 不产 argument buffer，tier2 bindless 不可用）。

#### Scenario: 压缩格式映射

- **WHEN** 创建 `BC7_UNORM_BLOCK` 图像
- **THEN** 映射到 `MTLPixelFormatBC7_RGBAUnorm`

#### Scenario: caps 上报

- **WHEN** 查询 `GetFeature()`
- **THEN** `descriptorHeap==false` 且 `firstInstanceIndirect==true`
