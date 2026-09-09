## MODIFIED Requirements

### Requirement: RenderGraph 三段式 lifecycle

`RenderGraph` SHALL 提供三段式：

- **Setup** — 声明资源与 pass 依赖（通过 builder）
- **Compile** — 推导依赖边、拓扑排序、生命周期、barrier、transient 池化；产出独立 `CompiledGraph`（扁平、只含 live pass、每 pass 一段连续 barrier）
- **Execute** — 依 compiled graph 拓扑序 emit barrier 与 pass body

`RenderGraph::Build(Device*, FrameAllocator&)` SHALL 接收 `FrameAllocator&`；setup graph 所有容器 SHALL 从 `FrameAllocator::Arena()` 分配。

`DeviceFrameContext` SHALL 持有 `FrameAllocator`，帧末统一 `Reset()` 回收所有帧数据。

#### Scenario: setup graph 从 FrameAllocator 分配
- **WHEN** `RenderGraph::Build(device, frameAlloc)` 建图
- **THEN** 所有节点/边/访问记录从 `frameAlloc.Arena()` 分配

#### Scenario: 编译产出 CompiledGraph
- **WHEN** `graph->Compile()`
- **THEN** 产出扁平 `CompiledGraph`；setup graph 可 `Rewind` 回收；executor 只读 `CompiledGraph`

#### Scenario: 帧末统一回收
- **WHEN** `DeviceFrameContext::EndFrame()` 调用 `FrameAllocator::Reset()`
- **THEN** 本帧所有 graph 数据（setup + compiled）被整体回收

## ADDED Requirements

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
