# aurora-rdg Specification

## Purpose
TBD - created by archiving change aurora-rdg. Update Purpose after archive.
## Requirements
### Requirement: RenderGraph 三段式 lifecycle

`RenderGraph` SHALL 提供三段式：Setup（builder 声明）→ Compile（产出 `CompiledGraph`）→ Execute（只读 CompiledGraph emit barrier 与 pass body）。其余条款不变。

#### Scenario: 三段式流程
- **WHEN** `Build(device, alloc)` → builder 声明 → `Compile()` → `Execute(cmdBuf)`
- **THEN** 产出 `CompiledGraph`；executor 按拓扑序 emit barrier 与 pass body

#### Scenario: executor 只读 CompiledGraph
- **WHEN** `ExecutePasses` 遍历 pass
- **THEN** 数据源为 `CompiledGraph.passes` / `CompiledGraph.barriers` / `CompiledGraph.resolvedImages|resolvedBuffers`；不访问 setup graph

#### Scenario: 帧末 barrier 段
- **WHEN** `CompiledGraph` 的 `finalBarrierOffset` 标记帧末 barrier 段起点
- **THEN** executor 在所有 pass 之后 emit `barriers[finalBarrierOffset .. end)`

### Requirement: Resource declaration: Import vs Create

`RenderGraph` SHALL 提供：

- `Import(name, ImagePtr) -> RDGTextureHandle` — 引用外部已存在的 image（如 SwapChain backbuffer），graph 持有 `ImagePtr` 引用
- `Import(name, BufferPtr) -> RDGBufferHandle` — 同上，持有 `BufferPtr` 引用
- `CreateTexture(name, RDGTextureDesc) -> RDGTextureHandle` — transient texture，由 RDG 池化
- `CreateBuffer(name, RDGBufferDesc) -> RDGBufferHandle`

其中 `name` 的类型为 `core::Name`；`ImagePtr` / `BufferPtr` 为 `CounterPtr<Image>` / `CounterPtr<Buffer>` 别名。

Import 资源由 graph 持有的智能指针引用保证存活，graph 生命周期内不被释放（graph 销毁时 `CounterPtr` 析构释放引用）；RDG 不会 delete 调用方创建的资源。Transient 资源在 graph 销毁时归还池。

#### Scenario: Import 外部 SwapChain image
- **WHEN** `auto bb = graph->Import(Name("backbuffer"), imagePtr)`，其中 `imagePtr` 是 `CounterPtr<Image>`
- **THEN** 返回有效 handle；后续 pass 可对 bb 做 ColorAttachment / Read；`imagePtr` 超出调用方作用域后底层 image 仍存活（graph 持有引用）

#### Scenario: CreateTexture 命中 transient pool
- **WHEN** 跨帧多次创建相同 desc 的 texture
- **THEN** transient pool 报告 hit；底层 RHI Image 复用（调试可观测 hit/miss 计数）

### Requirement: Pass 类型与 setup builder

`RenderGraph` SHALL 提供三种 pass 类型，每种对应一个 builder：

- `AddRasterPass(name, setup_fn, exec_fn)` — `RasterPassBuilder` 含 `ColorAttachment(slot, handle, loadOp, storeOp)`、`DepthStencilAttachment(handle, depthLoad, depthStore, stencilLoad, stencilStore)`、`Read(handle, AccessFlags)`、`SetViewport/Scissor`
- `AddComputePass(name, setup_fn, exec_fn)` — `ComputePassBuilder` 含 `Read(handle, AccessFlags)`、`Write(handle, AccessFlags)`
- `AddCopyPass(name, setup_fn, exec_fn)` — `CopyPassBuilder` 含 `Src(handle)`、`Dst(handle)`

setup callback 在 `AddXxxPass` 调用内立即执行；exec callback 在 `Execute()` 时被调用，签名为 `void(EncoderType &, RDGContext &)`。

#### Scenario: Raster pass 含 attachment + read texture
- **WHEN** Raster pass setup 声明 `ColorAttachment(0, gbuffer)` + `Read(shadowMap, FRAGMENT_SRV)`
- **THEN** Compile 期 RDG 知道：gbuffer 必须在此 pass 之前 transition 到 COLOR_ATTACHMENT；shadowMap 必须 transition 到 SHADER_READ_ONLY；本 pass 在 shadowMap-writer pass 之后执行

#### Scenario: Compute pass 写 storage buffer 后由 graphics pass 读
- **WHEN** ComputePass A 声明 `Write(buf, COMPUTE_UAV_WRITE)`；RasterPass B 声明 `Read(buf, FRAGMENT_SRV)`
- **THEN** Compile 推导：A 必须在 B 之前；A 与 B 之间 emit memoryBarrier(srcAccess=COMPUTE_UAV_WRITE, dstAccess=FRAGMENT_SRV, srcStage=COMPUTE_SHADER, dstStage=FRAGMENT_SHADER)

### Requirement: Compile 推导 barrier 并自动 emit

Compile 期 RDG SHALL 计算每个资源的"使用序列"（按 pass 顺序的 (access, layout) 列表）。相邻两次使用之间 RDG SHALL 在 Execute 时通过 `cmdBuf->PipelineBarrier(...)` emit 一组 barrier，含 srcAccess / dstAccess / srcStage / dstStage / oldLayout / newLayout。

调用方在 pass execute lambda 内 MUST NOT 自行调用 `cmdBuf->PipelineBarrier`；所有 barrier 由 RDG 提供。

第一次使用前从 `UNDEFINED` transition；如果资源在 graph 中被标记为 export（如 SwapChain image），最后一次使用后 transition 到 `PRESENT`。

#### Scenario: SwapChain image 三段 transition
- **WHEN** Import swapchain image → 用作 ColorAttachment → 标记 export with newLayout=PRESENT → Execute
- **THEN** Execute 顺序：barrier(UNDEFINED→COLOR_ATTACHMENT) → BeginRendering/Draw/EndRendering → barrier(COLOR_ATTACHMENT→PRESENT)；validation 不报错

#### Scenario: 同一资源连续两个 read pass 不重复发 barrier
- **WHEN** Pass A 写 image，Pass B 读，Pass C 也读（同 access）
- **THEN** A→B 之间发 barrier(write→read)；B→C 之间不发 barrier（access 一致，layout 已是 SHADER_READ_ONLY）

### Requirement: Pass culling 自动剔除未消费 pass

Compile 期 SHALL 从所有 export 资源（Import 进来的资源 + 显式 `MarkOfInterest(handle)` 标记）反向追溯依赖图：被任意 live pass 读的资源的 writer pass 也是 live。

不在 live 集合中的 pass 在 Execute 期被 SKIP（execute lambda 不被调用，barrier 不发出）。

#### Scenario: 写未读的 transient pass 被剔除
- **WHEN** Pass A 写 transient texture T，没有任何后续 pass 读 T，T 也未被 export
- **THEN** Pass A 在 Execute 期不被调用；T 不被分配实际 image

#### Scenario: 调试 logger 可输出剔除清单
- **WHEN** 启用 RDG debug 日志
- **THEN** Compile 完成时输出：live passes count + culled passes 列表 + 每个剔除原因（"no live consumer for outputs"）

### Requirement: Transient pool 复用底层 RHI 资源

RDG SHALL 维护 device-级 transient pool，按 (extent, format, samples, usage) hash 分桶。Compile 期对每个 transient handle 计算 [firstUsePass, lastUsePass] 区间，alloc 时优先复用：
- 同 hash 桶
- lastUsePass < 当前 handle 的 firstUsePass（生命周期不重叠）

的池中 image。未命中时创建新 image 入池。每帧结束后未被本帧使用的 image 在 N 帧（默认 3）后被淘汰。

#### Scenario: 同帧内不重叠 transient 复用
- **WHEN** 单帧内 4 个 transient texture 都是 1080p RGBA8，但 pass 串行使用（lifetime 不重叠）
- **THEN** Pool 仅持 1 个 RHI Image，4 个 RDG handle 共享底座；hit 计数 = 3

#### Scenario: 跨帧稳定命中
- **WHEN** 连续 N 帧使用相同 desc 的 transient texture
- **THEN** 第 2 帧起 pool hit；命中率监控接口报告 ≥ 99%

### Requirement: Execute 输出到单一 CommandBuffer

`RenderGraph::Execute(rhi::CommandBuffer *cmdBuf)` SHALL 把整个 graph 的 barrier、encoder、pass body 全部 emit 到给定的 cmdBuf 中。调用方负责 cmdBuf 的 Begin/End/Submit。

`Execute` 调用前 cmdBuf MUST 处于 recording 状态（`Begin()` 已调）；`Execute` 完成后 cmdBuf 仍处于 recording 状态（caller 后续 End + Submit）。

#### Scenario: Execute 后调用方 Submit
- **WHEN** cmdBuf->Begin → graph->Execute(cmdBuf) → cmdBuf->End → queue->Submit(cmdBuf, fence)
- **THEN** GPU 执行；fence 完成后所有 pass 的 GPU 工作已完成

### Requirement: Resource 与 pass 名称使用 core::Name

RDG 的资源名与 pass 名 SHALL 使用 `core::Name`（`sky::Name`）而非 `std::string`。所有 `name` 参数与内部存储（`ResourceNode::name`、`PassNode::name`）均为 `Name` 类型。

`Name` 提供 interning 语义：相同字符串对应相同 handle，`operator==` 为 O(1) 比较，`std::hash<Name>` 可用作 unordered 容器 key。

#### Scenario: 相同名称得到相同 Name 身份
- **WHEN** 两个 pass 用相同字符串字面量构造 `Name("post-process")`
- **THEN** 两个 `Name` 相等（`==` 成立），可作为一致的身份标识

### Requirement: CompiledPass variant payload

`CompiledPass` SHALL 使用 `std::variant<SceneRasterPayload, FullScreenPayload, ComputePayload, CopyBlitPayload, PresentPayload, CustomPayload>` 替代 `std::function` 字段。

`CompiledPassType` SHALL 扩展为 `{SCENE_RASTER, FULLSCREEN, COMPUTE, COPYBLIT, PRESENT, CUSTOM}`。

`CompiledGraph` SHALL 持有 `globalResourceGroup`（set 0，pipeline 全局一个）。

#### Scenario: 现有 raster pass 映射到 SCENE_RASTER
- **WHEN** `RenderGraph::AddRasterPass` 创建的 pass 经 `Compile()` 产出 `CompiledPass`
- **THEN** `CompiledPass.type == SCENE_RASTER`；`payload` 为 `SceneRasterPayload`（`items` 为空，后续 change 填充）

#### Scenario: executor switch dispatch
- **WHEN** `ExecutePasses` 遍历 `CompiledGraph.passes`
- **THEN** `switch (pass.type)` dispatch 到对应 payload 的 emit 逻辑

### Requirement: 基础 pass builder 与 API

`RenderGraph` SHALL 提供 6 种基础 pass 的 builder 与 `AddXxxPass` API：

- `AddSceneRasterPass(name, setup, exec)` → `SceneRasterPassBuilder`（`ColorAttachment` / `DepthStencilAttachment` / `AddDrawItem`）
- `AddFullScreenPass(name, setup)` → `FullScreenPassBuilder`（`SetTechnique` / `SetPassResourceGroup` / `SetTarget` / `SetDepthStencil` / `SetInputSRV`）
- `AddComputePass(name, setup)` → `ComputePassBuilder`（`SetPipeline` / `SetPassResourceGroup` / `SetGroups` / `Read` / `Write`）
- `AddCopyBlitPass(name, setup)` → `CopyBlitPassBuilder`（`Src` / `Dst` / `SetKind` / `SetSize` / `SetOffsets`）
- `AddPresentPass(name, setup)` → `PresentPassBuilder`（`SetSource`）
- `AddCustomPass(name, setup, exec)` → `CustomPassBuilder`（`SetCallback`）

`Compile.cpp::ProduceCompiledGraph` SHALL 从 `SceneRasterPassData.items` 拷贝到 `SceneRasterPayload.items`。

`Execute.cpp::ExecutePasses` SHALL `switch (pass.type)` dispatch 到各 payload 的 emit 逻辑（固定 set index 绑定：`set 1 = Pass ResourceGroup`，`set 2 = Batch ResourceGroup`）。

#### Scenario: SceneRasterPass 收集 DrawItem
- **WHEN** `SceneRasterPassBuilder.AddDrawItem(item)` 多次调用
- **THEN** `SceneRasterPassData.items` 按调用顺序收集；`Compile()` 后 `SceneRasterPayload.items` 包含相同顺序的 item

#### Scenario: FullScreenPass 构建
- **WHEN** `graph->AddFullScreenPass("bloom", [&](FullScreenPassBuilder &b) { b.SetTechnique(pso); b.SetPassResourceGroup(rg); b.SetTarget(tex); })`
- **THEN** 产出 `CompiledPass{type=FULLSCREEN, payload=FullScreenPayload{pso, rg, colors}}`；executor `BindResourceGroup(1, rg)` + `Draw(3,0,1,0)`

#### Scenario: CustomPass 逃生门
- **WHEN** `graph->AddCustomPass("dlss", setup, [](RDGContext &ctx, CommandBuffer &cmd) { /* device 扩展 */ })`
- **THEN** `CompiledPass{type=CUSTOM, payload=CustomPayload{fn}}`；executor 调用 `fn(ctx, cmd)`

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

`AddSceneRasterPass` SHALL 只接收 setup（`AddSceneRasterPass(name, setup)`），不再接收 execute lambda。execute 由 `SceneRasterPayload` 数据驱动。

`DrawItem` SHALL 新增 `uint32_t batchDynamicOffset`（默认 0）；executor 绑 set 2 SHALL 使用 `BindResourceGroup(2, item.batchResourceGroup, 1, &item.batchDynamicOffset)`（当 batch RG 非空时）。

#### Scenario: SceneRaster items 驱动绘制
- **WHEN** `SceneRasterPassBuilder.AddDrawItem(item)` 收集 item，`Compile()` 后 `Execute()`
- **THEN** executor 遍历 queues 的 items，逐 item `BindResourceGroup(2, batchRG, 1, &batchDynamicOffset)` / `BindPipeline` / `DrawIndexed`

### Requirement: Global ResourceGroup 生效

`RenderGraph` SHALL 提供 `SetGlobalResourceGroup(ResourceGroup*)`；`ProduceCompiledGraph` SHALL 写入 `CompiledGraph::globalResourceGroup`；executor SHALL 在每个 raster/fullscreen/compute pass 开始前 `BindResourceGroup(0, globalRG)`（globalRG 非空时）。

#### Scenario: global RG 绑定
- **WHEN** BuildRDG 时 `SetGlobalResourceGroup(rg)`，Compile 后 Execute
- **THEN** 每 pass 开始前 set 0 绑定该 RG

