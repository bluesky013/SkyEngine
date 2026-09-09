## ADDED Requirements

### Requirement: PipelinePass 模板基类

`PipelinePass` SHALL 提供三段式生命周期：`OnSetup(Device*)`（一次性创建持久 PSO/ResourceGroup）/ `BuildRDG(RenderGraph&)`（每帧构建 RDG 节点）/ `OnSceneChanged()`（显式重建持久资源）。

持久资源（PSO、ResourceGroup）由 pass 以成员持有；RDG 层不缓存、不管理其生命周期。

#### Scenario: 场景不变时持久资源复用
- **WHEN** pass 连续多帧 `BuildRDG` 且未调 `OnSceneChanged`
- **THEN** 每帧 RDG 节点重建，但 PSO / ResourceGroup 复用同一实例

### Requirement: OpaquePass

`OpaquePass` SHALL 继承 `SceneRasterPassTemplate`，声明 color + depth attachment 与一个 `opaque` queue（`FRONT_TO_BACK` 排序标记）。v1 无 scene 集成（Collect 为空钩子）。

#### Scenario: OpaquePass 构建 RDG
- **WHEN** `opaquePass.BuildRDG(graph)`
- **THEN** graph 中出现一个 SceneRasterPass（color+depth attachment），其 queues 包含名为 "opaque" 的 queue
