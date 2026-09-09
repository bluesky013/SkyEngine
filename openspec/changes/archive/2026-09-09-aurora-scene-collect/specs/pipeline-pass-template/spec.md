## MODIFIED Requirements

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
