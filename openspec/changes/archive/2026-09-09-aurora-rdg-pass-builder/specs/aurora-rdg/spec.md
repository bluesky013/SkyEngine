## MODIFIED Requirements

### Requirement: RenderGraph 三段式 lifecycle

`RenderGraph` SHALL 提供三段式：

- **Setup** — 声明资源与 pass 依赖（通过 builder）
- **Compile** — 推导依赖边、拓扑排序、生命周期、barrier、transient 池化；产出独立 `CompiledGraph`
- **Execute** — 依 compiled graph 拓扑序 emit barrier 与 pass body

`RenderGraph::Build(Device*, FrameAllocator&)` SHALL 接收 `FrameAllocator&`；setup graph 所有容器 SHALL 从 `FrameAllocator::Arena()` 分配。

#### Scenario: 三段式流程
- **WHEN** `Build(device, alloc)` → builder 声明 → `Compile()` → `Execute(cmdBuf)`
- **THEN** 产出 `CompiledGraph`；executor 按拓扑序 emit barrier 与 pass body

## ADDED Requirements

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
