# aurora-pipeline Specification

## Purpose
TBD - consolidated from: aurora-renderpass aurora-render-viewport aurora-scene-collect pipeline-pass-template
## Requirements

### Requirement: Render pass begin/end 声明契约

Pipeline 层 pass SHALL 通过 color attachment（槽位 + handle + load/store + clear）与 depth/stencil attachment（handle + depth/stencil load/store + clear）以及 render area 声明 render target，**不**手写 `BeginRendering`/`EndRendering`。RDG executor SHALL 在编译后为 SCENE_RASTER 与 FULLSCREEN pass 统一生成 `BeginRendering(RenderingInfo)` 与 `EndRendering()`。

#### Scenario: SceneRasterPass 生成 begin/end

- **WHEN** 一个 `SceneRasterPassTemplate` 子类的 `BuildRDG` 声明了 color + depth attachment 并 `Compile()` + `Execute()` 后
- **THEN** executor 在遍历该 pass 的 queue items 之前调用 `BeginRendering`，在之后调用 `EndRendering`，且 `RenderingInfo` 的 attachment 数量、load/store 与声明一致

### Requirement: ScenePass (HDR)

`ScenePass` SHALL 继承 `SceneRasterPassTemplate`，在 `BuildRDG` 中声明一个 HDR color target（`RGBA16_SFLOAT`，`RENDER_TARGET | TRANSFER_SRC` usage）+ depth target（`D32`，`DEPTH_STENCIL` usage），color 与 depth 均以 `CLEAR` load，并声明一个名为 `opaque` 的 queue（`FRONT_TO_BACK`）。`ScenePass` SHALL 暴露 `GetHDRColorHandle()` 与 `GetDepthHandle()`，并提供 `SetExtent(width, height)`。

#### Scenario: ScenePass 构建 HDR RDG 节点

- **WHEN** `scenePass.SetExtent(1280, 720); scenePass.BuildRDG(graph); graph->Compile()`
- **THEN** compiled graph 中存在一个 `SCENE_RASTER` pass，其 color attachment 为 HDR 格式且 loadOp 为 `CLEAR`，depth attachment loadOp 为 `CLEAR`，且 queues 含名为 `opaque` 的 queue（`FRONT_TO_BACK`）

### Requirement: TextureToScreenPass

`TextureToScreenPass` SHALL 继承 `PipelinePass`，在 `BuildRDG` 中声明一个 FullScreenPass：以 `SetInputSRV(inputHandle)` 建立对输入 HDR texture 的 SRV read 依赖，以 `SetTarget(outputHandle, DONT_CARE, STORE)` 声明输出 color attachment（全屏覆盖）。`TextureToScreenPass` SHALL 暴露 `SetInput(RDGTextureHandle)` 与 `SetOutput(RDGTextureHandle)`。

#### Scenario: TextureToScreen 构建 fullscreen 节点

- **WHEN** `pass.SetInput(hdr); pass.SetOutput(backbuffer); pass.BuildRDG(graph); graph->Compile()`
- **THEN** compiled graph 中存在一个 `FULLSCREEN` pass，其 color attachment 指向 output 且 loadOp 为 `DONT_CARE`，并且输入 HDR texture 与该 pass 存在 SRV read 依赖（供 barrier/culling 推导）

#### Scenario: 输入 HDR texture 从 RTV 转 SRV

- **WHEN** `ScenePass` 写出 HDR color 后由 `TextureToScreenPass` 以 SRV 读取，且 RDG `Compile()` 完成
- **THEN** 两 pass 之间对 HDR texture 产生从 RTV 到 SRV 的 barrier（由 `SetInputSRV` 的 read 依赖驱动），保证先写后读


### Requirement: RenderViewport 三阶段生命周期契约

`RenderViewport` SHALL 定义三阶段生命周期：`Begin()`（check swapchain 状态 + 重建，false = 取消当帧 present）、`Acquire()`（拿下一帧 backbuffer，false = 本帧不可渲染）、`Release()`（present，取消帧为 no-op）。

`RenderViewport` SHALL 暴露：`GetBackbuffer()`（Acquire 成功后有效）、`GetFormat()`、`GetExtent()`、`GetName()`，以及供 Submit 用的 `GetAcquireSemaphore()` / `GetRenderDoneSemaphore()`（当前帧 ring 的 BINARY sema）。基类 SHALL NOT 暴露 frame index 或 fence（全局化，见「全局 inflight frame」需求）。

#### Scenario: Begin 失败取消 present

- **WHEN** `viewport->Begin()` 返回 false（swapchain 状态异常且重建失败）
- **THEN** 帧驱动方跳过本帧（不建 RDG、不 Submit、不 Present）

#### Scenario: Acquire 后 backbuffer 有效

- **WHEN** `viewport->Begin()` 成功且 `viewport->Acquire()` 返回 true
- **THEN** `GetBackbuffer()` 返回非空 `Image*`（持有 RENDER_TARGET），`GetFormat()`/`GetExtent()` 与表面一致

### Requirement: ClientViewport 持有 SwapChain 与 per-image sema

`ClientViewport` SHALL 继承 `RenderViewport`，`Init(Device*, const SwapChain::Descriptor&)` 创建 SwapChain 并分配 `imageCount` 组 acquire/render-done BINARY semaphore（按 per-viewport 帧 ring 索引，本地 acquire 计数器）。

`Begin()` SHALL 依据 `GetStatus()` 重建；`Acquire()` SHALL 调 `AcquireNextImage(acquireSema[slot], nullptr, UINT64_MAX)` 并记 image index；`Release()` SHALL 在帧有效时调 `Present(imageIndex, 1, &renderDoneSema[slot])`。`ClientViewport` SHALL NOT 持有 frame index / fence（全局化）。

#### Scenario: Acquire 返回有效索引

- **WHEN** `ClientViewport::Init` 成功后 `Begin()` + `Acquire()`
- **THEN** `Acquire()` 返回 true；backbuffer 非空；acquire sema / render-done sema 均非空

#### Scenario: 取消帧的 Release 为 no-op

- **WHEN** `Begin()` 或 `Acquire()` 失败后调用 `Release()`
- **THEN** 不调用 `SwapChain::Present`，不崩溃

### Requirement: SwapChainStatus 自查与重建

`ClientViewport::Begin()` SHALL 查询 `SwapChain::GetStatus()`：`OK` → 直接继续；`OUT_OF_DATE` → 调 `Resize` 重建；`LOST` 或 `Resize` 失败 → 取消当帧 present。surface 尺寸由原生窗口决定，RHI 层不决定目标尺寸；`SUBOPTIMAL` SHALL 当作 `OK` 处理。

#### Scenario: OUT_OF_DATE 触发重建

- **WHEN** 窗口尺寸变化导致 `GetStatus()` 返回 `OUT_OF_DATE`
- **THEN** `Begin()` 触发 `Resize`；重建成功后本帧正常渲染；`GetExtent()` 反映新尺寸

#### Scenario: 重建失败取消 present

- **WHEN** `GetStatus()` 返回 `LOST` 或 `Resize` 返回失败
- **THEN** `Begin()` 返回 false；本帧 present 被取消，不崩溃

### Requirement: 全局 inflight frame 共享

`DeviceFrameContext` SHALL 持有全局 `mFrameIndex` + `mInflightNum` 个 fence + `FrameAllocator`。`BeginFrame()` SHALL `Wait` 并 `Reset` `fence[mFrameIndex % N]` 后自增 `mFrameIndex`；共享 dynamic buffer（`BatchAllocator`/`GlobalRenderResources`/`TransientBufferPool`）SHALL 按 `mFrameIndex % N` 分配。

同一帧内多个 viewport SHALL 共享同一 `mFrameIndex`；各 viewport 的 `SubmitInfo.fence` SHALL 使用全局 `fence[mFrameIndex % N]`。`GetFrameFence()` SHALL 返回当前帧 fence。

#### Scenario: 多 viewport 同帧共享 dynamic buffer 稳定

- **WHEN** 一帧内主窗 + 编辑器小窗两个 viewport 各自 Acquire/Render/Release，共享的 dynamic buffer 按同一 `mFrameIndex` 分配
- **THEN** 两 viewport 读写同一帧的 dynamic buffer 不互相覆盖（fence 保证 N 帧前的 GPU 工作完成）

### Requirement: 各后端 present 映射

`ClientViewport` SHALL 只依赖 `SwapChain`/`Queue`/`Semaphore`/`Fence` 抽象，后端无关。present 差异封闭在各后端 `SwapChain` 实现：

- Vulkan：acquire `vkAcquireNextImageKHR` signal binary；render-done submit signal binary；present `vkQueuePresentKHR` wait。
- DX12：acquire `GetCurrentBackBufferIndex`（acquire sema 立即 signal）；render-done `ID3D12Fence::Signal`；present `IDXGISwapChain3::Present`。
- Metal：acquire `nextDrawable` + 立即 signal；render-done `MTLEvent` signal；present `presentDrawable:` 前 `encodeWaitForEvent`。

#### Scenario: 三后端接口一致

- **WHEN** 同一 `ClientViewport` 调用序列跑在 Vulkan / DX12 / Metal 任一后端
- **THEN** `Begin`/`Acquire`/`Release` 接口与顺序一致，后端差异不泄漏到调用方


### Requirement: SceneView（aurora）

`SceneView` SHALL 提供 frustum（6 plane）+ view/viewProject 矩阵，以及 `FrustumCulling(const AABB&) -> bool`。v1 只为 culling 与排序服务，不涉及常量上传。

#### Scenario: frustum 剔除
- **WHEN** primitive 的 worldBounds 完全在 view frustum 外
- **THEN** `FrustumCulling(bounds)` 返回 false，该 primitive 被剔除

### Requirement: RenderPrimitive（aurora）

`RenderPrimitive` SHALL 持有 geometry（vb/ib/offsets/draw args）+ worldBounds + 按 technique tag 索引的 `TechniqueBinding`（`pso` + `batchResourceGroup`）映射，并提供 `GatherRenderItem(context)`：context.tag 命中持有的 tag 才 append `DrawItem`；context.tag 为空时不过滤（append 默认 binding）。

primitive 为持久对象，其内部容器 SHALL NOT 绑定帧 arena。

#### Scenario: tag 过滤收集
- **WHEN** primitive 持有 "opaque" 与 "shadow" 两份 binding，收集 context.tag = "opaque"
- **THEN** 只 append opaque binding 对应的 DrawItem；shadow binding 不被收集

#### Scenario: 空 tag 不过滤
- **WHEN** 收集 context.tag 为空
- **THEN** primitive append 其默认 binding（保持默认 queue 兼容）

### Requirement: RenderScene（aurora）

`RenderScene` SHALL 内嵌 `EntityRegistry`，提供 entity 注册与 SoA 组件池能力（`CreateEntity` / `DestroyEntity` / `Add<T>` / `Get<T>` / `Pool<T>` / `View<Ts...>`）；views（SceneView）保留独立 registry 不进 ECS。

场景组件（`aurora/scene/SceneTypes.h`）SHALL 包括：

- `Bounds`（AABB）
-  `WorldInfo`（纯 world 矩阵：`Matrix4 world`，默认 Identity；不拆 TRS）
- `Light`（type/color/intensity + point/spot 参数：`position` / `range` / `innerConeAngle` / `outerConeAngle`）
- `Skin`（占位）

#### Scenario: ECS 组件挂载
- **WHEN** `scene.CreateEntity()` 后 `scene.Add<Light>(id, {...})`
- **THEN** Light 存入对应 SoA 池；`scene.DestroyEntity(id)` 后移除

#### Scenario: 收集链路不受影响
- **WHEN** pass BuildRDG 触发 Collect
- **THEN** 仍遍历 `Bounds`（经 `View<Bounds>`），frustum cull 语义与现状一致

#### Scenario: point/spot 参数
- **WHEN** `Light{type=POINT, position, range}` 或 `Light{type=SPOT, position, direction, range, innerConeAngle, outerConeAngle}`
- **THEN** 各参数完整存储于组件

#### Scenario: WorldInfo 矩阵存储
- **WHEN** `scene.Add<WorldInfo>(id, {matrix})` 后 `scene.Get<WorldInfo>(id)`
- **THEN** 读回的 world 矩阵与写入一致

### Requirement: pass 收集链路

`SceneRasterPassTemplate::Collect` SHALL 以 `Bounds` 池的 dense 数组为主驱动遍历（连续扫描），按 entity 查询 `RenderItem`（sparse 索引，无哈希）；tag 过滤、frustum cull、排序语义不变。

#### Scenario: dense 遍历收集
- **WHEN** Collect 遍历场景
- **THEN** bounds 数组连续扫描（无指针跳转）；RenderItem 按 entity 下标索引（int 索引，无 Name 哈希）


### Requirement: PipelinePass 模板基类

`PipelinePass` SHALL 提供三段式生命周期：`OnSetup(Device*)`（一次性创建持久 PSO/ResourceGroup）/ `BuildRDG(RenderGraph&)`（每帧构建 RDG 节点）/ `OnSceneChanged()`（显式重建持久资源）。

持久资源（PSO、ResourceGroup）由 pass 以成员持有；RDG 层不缓存、不管理其生命周期。

`SceneRasterPassTemplate` SHALL 额外提供 `SetScene(RenderScene*)` / `SetView(SceneView*)` 与 `Collect(builder)` 默认实现（tag 过滤 + frustum cull + 排序，见 `aurora-scene-collect` capability）。

#### Scenario: 场景不变时持久资源复用
- **WHEN** pass 连续多帧 `BuildRDG` 且未调 `OnSceneChanged`
- **THEN** 每帧 RDG 节点重建，但 PSO / ResourceGroup 复用同一实例

#### Scenario: pass 绑定场景与视图
- **WHEN** `pass.SetScene(scene); pass.SetView(view); pass.BuildRDG(graph)`
- **THEN** Collect 使用绑定的 scene/view 收集该 pass 的 queue items

### Requirement: OpaquePass

`OpaquePass` SHALL 继承 `SceneRasterPassTemplate`，声明 color + depth attachment 与一个 `opaque` queue（`FRONT_TO_BACK` 排序标记）。v1 无 scene 集成（Collect 为空钩子）。

#### Scenario: OpaquePass 构建 RDG
- **WHEN** `opaquePass.BuildRDG(graph)`
- **THEN** graph 中出现一个 SceneRasterPass（color+depth attachment），其 queues 包含名为 "opaque" 的 queue

