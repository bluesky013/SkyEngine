## MODIFIED Requirements

### Requirement: RenderGraph 三段式 lifecycle

`RenderGraph` SHALL 提供三段式：

- **Setup** — 声明资源与 pass 依赖（通过 builder）
- **Compile** — 推导依赖边、拓扑排序、生命周期、barrier、transient 池化；产出独立 `CompiledGraph`
- **Execute** — 依 `CompiledGraph` 拓扑序 emit barrier 与 pass body；**只读 `CompiledGraph`，不触碰 setup graph**

#### Scenario: 三段式流程
- **WHEN** `Build(device, alloc)` → builder 声明 → `Compile()` → `Execute(cmdBuf)`
- **THEN** 产出 `CompiledGraph`；executor 按拓扑序 emit barrier 与 pass body

#### Scenario: executor 只读 CompiledGraph
- **WHEN** `ExecutePasses` 遍历 pass
- **THEN** 数据源为 `CompiledGraph.passes` / `CompiledGraph.barriers` / `CompiledGraph.resolvedImages|resolvedBuffers`；不访问 setup graph 的 `mPasses`/`mTopoOrder`/pass data

#### Scenario: 帧末 barrier 段
- **WHEN** `CompiledGraph` 的 `finalBarrierOffset` 标记帧末 barrier 段起点
- **THEN** executor 在所有 pass 之后 emit `barriers[finalBarrierOffset .. end)`

## ADDED Requirements

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

`AddSceneRasterPass` SHALL 只接收 setup（`AddSceneRasterPass(name, setup)`），不再接收 execute lambda。execute 由 `SceneRasterPayload.items` 数据驱动：`BindResourceGroup(2, batchRG)` / `BindPipeline(pso)` / `BindIndexBuffer` / `DrawIndexed`。

`ComputePayload` / `CopyBlitPayload` MAY 保留 `executeFn` 字段作为过渡期 fallback。

#### Scenario: SceneRaster items 驱动绘制
- **WHEN** `SceneRasterPassBuilder.AddDrawItem(item)` 收集 item，`Compile()` 后 `Execute()`
- **THEN** executor 遍历 `SceneRasterPayload.items`，逐 item `BindResourceGroup(2)`/`BindPipeline`/`DrawIndexed`

#### Scenario: Compute executeFn fallback
- **WHEN** `AddComputePass(name, setup, executeFn)`，`Compile()` 后 `Execute()`
- **THEN** `ComputePayload.executeFn` 被调用（过渡期）
