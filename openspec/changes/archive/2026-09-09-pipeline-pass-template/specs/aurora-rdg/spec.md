## MODIFIED Requirements

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

`AddSceneRasterPass` SHALL 只接收 setup（`AddSceneRasterPass(name, setup)`），不再接收 execute lambda。execute 由 `SceneRasterPayload` 数据驱动。

SceneRasterPass SHALL 支持多 queue：每 queue 有独立 `items`、queue 级 `ResourceGroup`、排序策略标记（`NONE` / `FRONT_TO_BACK` / `BACK_TO_FRONT`）。`SceneRasterPassData::items` SHALL 改为 `queues: TransientVector<SceneRasterQueue>`；`SceneRasterPayload::items` SHALL 改为 `queues`。

`SceneRasterPassBuilder` SHALL 提供 `AddQueue(const Name&) -> uint32_t` / `AddDrawItem(uint32_t queue, const DrawItem&)` / `SetQueueResourceGroup(uint32_t queue, ResourceGroup*)`；无 queue 参数的 `AddDrawItem(item)` SHALL 路由到默认 queue 0（懒创建）。

executor SHALL 按声明序遍历 queue：`BindResourceGroup(1, queueRG 或 passRG)` → 遍历 items：`BindResourceGroup(2, batchRG)` / `BindPipeline(pso)` / `DrawIndexed`。

RDG SHALL NOT 做 queue 内排序（sortPolicy 为纯数据标记，排序由收集方完成，RDG 保序执行）。

#### Scenario: 多 queue 保序执行
- **WHEN** builder 依次 `AddQueue("opaque")` / `AddQueue("transparent")` 并各自 AddDrawItem
- **THEN** `SceneRasterPayload.queues` 按声明序排列；executor 先执行 opaque queue 的 items，再 transparent

#### Scenario: queue 级 ResourceGroup
- **WHEN** queue 设置了 queueResourceGroup
- **THEN** executor 在进入该 queue 时 `BindResourceGroup(1, queueRG)`；未设置时用 pass 级 RG

#### Scenario: 默认 queue 兼容
- **WHEN** 调用无 queue 参数的 `AddDrawItem(item)` 且未显式 AddQueue
- **THEN** 懒创建默认 queue 0（name="default"），item 进入该 queue
