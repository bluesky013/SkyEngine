# aurora-scene-collect Specification

## Purpose
TBD - created by archiving change aurora-scene-collect. Update Purpose after archive.
## Requirements
### Requirement: SceneView（aurora）

`SceneView` SHALL 提供 frustum（6 plane）+ view/viewProject 矩阵，以及 `FrustumCulling(const AABB&) -> bool`。v1 只为 culling 与排序服务，不涉及常量上传。

#### Scenario: frustum 剔除
- **WHEN** primitive 的 worldBounds 完全在 view frustum 外
- **THEN** `FrustumCulling(bounds)` 返回 false，该 primitive 被剔除

### Requirement: RenderPrimitive（aurora）

`RenderPrimitive` SHALL 持有 geometry（vb/ib/offsets/draw args）+ worldBounds + 按 technique tag 索引的 `TechniqueBinding`（`pso` + `batchResourceGroup`）映射，并提供 `GatherRenderItem(context)`：context.tag 命中持有的 tag 才 append `DrawItem`；context.tag 为空时不过滤（append 默认 binding）。

primitive 为持久对象，其内部容器 SHALL NOT 绑定帧 arena。

#### Scenario: tag 过滤收集
- **WHEN** primitive 持有 "opaque" 与 "shadow" 两份 binding，收集 context.tag = "opaque"
- **THEN** 只 append opaque binding 对应的 DrawItem；shadow binding 不被收集

#### Scenario: 空 tag 不过滤
- **WHEN** 收集 context.tag 为空
- **THEN** primitive append 其默认 binding（保持默认 queue 兼容）

### Requirement: RenderScene（aurora）

`RenderScene` SHALL 内嵌 `EntityRegistry`，提供 entity 注册与 SoA 组件池能力（`CreateEntity` / `DestroyEntity` / `Add<T>` / `Get<T>` / `Pool<T>` / `View<Ts...>`）；views（SceneView）保留独立 registry 不进 ECS。

场景组件（`aurora/scene/SceneTypes.h`）SHALL 包括：

- `Bounds`（AABB）
-  `WorldInfo`（纯 world 矩阵：`Matrix4 world`，默认 Identity；不拆 TRS）
- `Light`（type/color/intensity + point/spot 参数：`position` / `range` / `innerConeAngle` / `outerConeAngle`）
- `Skin`（占位）

#### Scenario: ECS 组件挂载
- **WHEN** `scene.CreateEntity()` 后 `scene.Add<Light>(id, {...})`
- **THEN** Light 存入对应 SoA 池；`scene.DestroyEntity(id)` 后移除

#### Scenario: 收集链路不受影响
- **WHEN** pass BuildRDG 触发 Collect
- **THEN** 仍遍历 `Bounds`（经 `View<Bounds>`），frustum cull 语义与现状一致

#### Scenario: point/spot 参数
- **WHEN** `Light{type=POINT, position, range}` 或 `Light{type=SPOT, position, direction, range, innerConeAngle, outerConeAngle}`
- **THEN** 各参数完整存储于组件

#### Scenario: WorldInfo 矩阵存储
- **WHEN** `scene.Add<WorldInfo>(id, {matrix})` 后 `scene.Get<WorldInfo>(id)`
- **THEN** 读回的 world 矩阵与写入一致

### Requirement: pass 收集链路

`SceneRasterPassTemplate::Collect` SHALL 以 `Bounds` 池的 dense 数组为主驱动遍历（连续扫描），按 entity 查询 `RenderItem`（sparse 索引，无哈希）；tag 过滤、frustum cull、排序语义不变。

#### Scenario: dense 遍历收集
- **WHEN** Collect 遍历场景
- **THEN** bounds 数组连续扫描（无指针跳转）；RenderItem 按 entity 下标索引（int 索引，无 Name 哈希）

