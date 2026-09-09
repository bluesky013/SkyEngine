## MODIFIED Requirements

### Requirement: RenderScene（aurora）

`RenderScene` SHALL 基于 `EntityRegistry` + SoA 组件池存储场景数据（ECS data-oriented），不再使用 `std::vector<RenderPrimitive*>` 指针模型；`RenderPrimitive` 独立结构 SHALL 删除。

场景组件（`scene/SceneTypes.h`）SHALL 包括：`Bounds`（AABB）、`RenderItem`（过渡形态：`Name techniqueTag` + `DrawItem item`，pso/batchRG/vb/ib/args 全在 item 内）、`Light`（占位）、`Skin`（占位）。

外部引用 SHALL 使用 `EntityId`。views 保留独立 registry（view 数量少，不进 ECS）。

#### Scenario: entity 注册与组件挂载
- **WHEN** `scene.CreateEntity()` 后挂载 Bounds/RenderItem 组件
- **THEN** 组件存入对应 SoA 池；`scene.DestroyEntity(id)` 后组件全部移除

#### Scenario: tag 过滤语义保持
- **WHEN** entity 的 `RenderItem.techniqueTag = "opaque"`，queue 的 techniqueTag = "shadow"
- **THEN** 该 entity 不被 shadow queue 收集；被 "opaque" queue 收集

#### Scenario: 空 tag queue 不过滤
- **WHEN** queue 的 techniqueTag 为空
- **THEN** 所有 visible entity 的 RenderItem 都被收集（与旧默认 queue 语义一致）

### Requirement: pass 收集链路

`SceneRasterPassTemplate::Collect` SHALL 以 `Bounds` 池的 dense 数组为主驱动遍历（连续扫描），按 entity 查询 `RenderItem`（sparse 索引，无哈希）；tag 过滤、frustum cull、排序语义不变。

#### Scenario: dense 遍历收集
- **WHEN** Collect 遍历场景
- **THEN** bounds 数组连续扫描（无指针跳转）；RenderItem 按 entity 下标索引（int 索引，无 Name 哈希）
