## MODIFIED Requirements

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

`AddSceneRasterPass` SHALL 只接收 setup（`AddSceneRasterPass(name, setup)`），不再接收 execute lambda。execute 由 `SceneRasterPayload` 数据驱动。

SceneRasterPass SHALL 支持多 queue：每 queue 有独立 `items`、queue 级 `ResourceGroup`、排序策略标记（`NONE` / `FRONT_TO_BACK` / `BACK_TO_FRONT`）与 **`techniqueTag`（`Name`，纯数据字段，RDG 不消费，收集方用作过滤器）**。

`SceneRasterPassBuilder` SHALL 提供 `AddQueue(const Name&, QueueSortPolicy, const Name &tag)` 三参重载；旧两参重载保留（tag 为空）。

#### Scenario: queue 携带 technique tag
- **WHEN** `AddQueue("opaque", FRONT_TO_BACK, "opaque")`
- **THEN** `SceneRasterQueue.techniqueTag == "opaque"`；CompiledGraph 中该字段原样保留

#### Scenario: 空 tag 默认行为
- **WHEN** 用两参 `AddQueue("default", NONE)`（无 tag）
- **THEN** `techniqueTag` 为空；收集方视为不过滤

（其余条款不变：多 queue 保序执行、queue 级 ResourceGroup、默认 queue 兼容、RDG 不做排序。）
