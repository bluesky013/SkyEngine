## ADDED Requirements

### Requirement: RenderGraph 三段式 lifecycle

`RenderGraph` SHALL 提供三个明确分离的阶段：

1. **Setup**：调用方添加 pass，每个 pass 在 setup callback 内通过 builder 声明读写、attachment
2. **Compile**：拓扑排序、生命周期分析、transient 池分配、barrier 推导、pass culling
3. **Execute**：按拓扑顺序把 barrier 与 pass body emit 到一个 CommandBuffer

调用方 MUST 按 setup → compile → execute 顺序调用。重复调用 Compile 与 Execute 之间必须 Reset 或重新 Build。

#### Scenario: 单 pass 完整 lifecycle
- **WHEN** 调用方 `RenderGraph::Build(device)` → `AddRasterPass(name, setup, exec)` → `Compile()` → `Execute(cmdBuf)` → cmdBuf submit
- **THEN** 不报错；pass 的 execute lambda 在 Execute 期间被调用一次

#### Scenario: 顺序错误被拒绝
- **WHEN** 调用方在 `Compile()` 之前调 `Execute()`
- **THEN** Debug build assert；Release build 行为未定义但 MUST 不静默成功

### Requirement: Resource declaration: Import vs Create

`RenderGraph` SHALL 提供：
- `Import(name, rhi::Image*) -> RDGTextureHandle` — 引用外部已存在的 image（如 SwapChain backbuffer）
- `Import(name, rhi::Buffer*) -> RDGBufferHandle`
- `CreateTexture(name, RDGTextureDesc) -> RDGTextureHandle` — transient texture，由 RDG 池化
- `CreateBuffer(name, RDGBufferDesc) -> RDGBufferHandle`

Import 资源的生命周期由调用方管理；RDG 不会销毁。Transient 资源在 graph 销毁时归还池。

#### Scenario: Import 外部 SwapChain image
- **WHEN** `auto bb = graph->Import("backbuffer", swapchain->GetImage(idx))`
- **THEN** 返回有效 handle；后续 pass 可对 bb 做 ColorAttachment / Read

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
