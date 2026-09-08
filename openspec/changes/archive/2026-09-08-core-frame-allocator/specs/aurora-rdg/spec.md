## MODIFIED Requirements

### Requirement: RenderGraph 三段式 lifecycle

`RenderGraph` SHALL 提供三段式：

- **Setup** — 声明资源与 pass 依赖
- **Compile** — 推导依赖边、拓扑排序、生命周期、barrier、transient 池化；产出独立 `CompiledGraph`（扁平、只含 live pass、每 pass 一段连续 barrier）
- **Execute** — 依 compiled graph 拓扑序 emit barrier 与 pass body

`RenderGraph::Build(Device*, FrameAllocator&)` SHALL 接收 `FrameAllocator&`；setup graph 所有容器（`std::vector`/`std::string`）SHALL 迁移到 `TransientVector`/`TransientHashMap`（从 `FrameAllocator::Arena()` 分配）。

`DeviceFrameContext` SHALL 持有 `FrameAllocator`，帧末统一 `Reset()` 回收所有帧数据。

#### Scenario: setup graph 从 FrameAllocator 分配
- **WHEN** `RenderGraph::Build(device, frameAlloc)` 建图
- **THEN** 所有节点/边/访问记录从 `frameAlloc.Arena()` 分配，`frameAlloc.GetCurrentBytes()` 增长

#### Scenario: 编译产出 CompiledGraph
- **WHEN** `graph->Compile()`
- **THEN** 产出扁平 `CompiledGraph`；setup graph 可 `Rewind` 回收；executor 只读 `CompiledGraph`

#### Scenario: 帧末统一回收
- **WHEN** `DeviceFrameContext::EndFrame()` 调用 `FrameAllocator::Reset()`
- **THEN** 本帧所有 graph 数据（setup + compiled）被整体回收，`GetCurrentBytes() == 0`
