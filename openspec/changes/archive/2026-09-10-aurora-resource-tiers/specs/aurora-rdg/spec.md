## MODIFIED Requirements

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

`AddSceneRasterPass` SHALL 只接收 setup（`AddSceneRasterPass(name, setup)`），不再接收 execute lambda。execute 由 `SceneRasterPayload` 数据驱动。

`DrawItem` SHALL 新增 `uint32_t batchDynamicOffset`（默认 0）；executor 绑 set 2 SHALL 使用 `BindResourceGroup(2, item.batchResourceGroup, 1, &item.batchDynamicOffset)`（当 batch RG 非空时）。

#### Scenario: SceneRaster items 驱动绘制
- **WHEN** `SceneRasterPassBuilder.AddDrawItem(item)` 收集 item，`Compile()` 后 `Execute()`
- **THEN** executor 遍历 queues 的 items，逐 item `BindResourceGroup(2, batchRG, 1, &batchDynamicOffset)` / `BindPipeline` / `DrawIndexed`

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

## ADDED Requirements

### Requirement: Global ResourceGroup 生效

`RenderGraph` SHALL 提供 `SetGlobalResourceGroup(ResourceGroup*)`；`ProduceCompiledGraph` SHALL 写入 `CompiledGraph::globalResourceGroup`；executor SHALL 在每个 raster/fullscreen/compute pass 开始前 `BindResourceGroup(0, globalRG)`（globalRG 非空时）。

#### Scenario: global RG 绑定
- **WHEN** BuildRDG 时 `SetGlobalResourceGroup(rg)`，Compile 后 Execute
- **THEN** 每 pass 开始前 set 0 绑定该 RG
