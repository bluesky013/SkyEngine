## MODIFIED Requirements

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
