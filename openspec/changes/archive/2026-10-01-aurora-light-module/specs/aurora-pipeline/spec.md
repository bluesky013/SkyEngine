## MODIFIED Requirements

### Requirement: RenderScene（aurora）

`RenderScene` SHALL 内嵌 `EntityRegistry`，提供 entity 注册与 SoA 组件池能力（`CreateEntity` / `DestroyEntity` / `Add<T>` / `Get<T>` / `Pool<T>` / `View<Ts...>`）；views（SceneView）保留独立 registry 不进 ECS。

场景组件 SHALL 包括：

- `Bounds`（`BoundingBoxSphere`）
- `WorldInfo`（纯 world 矩阵：`Matrix4 world`，默认 Identity；不拆 TRS）
- `Light`（声明于 `aurora/light/LightTypes.h`，由 `aurora/scene/SceneTypes.h` 转发：type/color/intensity + point/spot 参数 `range` / `innerConeAngle` / `outerConeAngle`；**不**存储 `position` / `direction`，世界位置 / 朝向由 `WorldInfo` 变换派生）
- `MainLight`（`aurora/light/LightTypes.h`，注册组件：把某个 `Light{DIRECTIONAL}` entity 标记为场景主光，携带 `castShadow` 等；`RenderScene` **不**缓存主光）
- `Skin`（占位）

#### Scenario: ECS 组件挂载

- **WHEN** `scene.CreateEntity()` 后 `scene.Add<Light>(id, {...})`
- **THEN** Light 存入对应 SoA 池；`scene.DestroyEntity(id)` 后移除

#### Scenario: 收集链路不受影响

- **WHEN** pass BuildRDG 触发 Collect
- **THEN** 仍遍历 `Bounds`（经 `View<Bounds>`），frustum cull 语义与现状一致

#### Scenario: point/spot 参数

- **WHEN** `Light{type=POINT, range}` 或 `Light{type=SPOT, range, innerConeAngle, outerConeAngle}`
- **THEN** 各参数完整存储于组件；位置 / 朝向不存于组件，而从同 entity 的 `WorldInfo.world` 派生

#### Scenario: WorldInfo 矩阵存储

- **WHEN** `scene.Add<WorldInfo>(id, {matrix})` 后 `scene.Get<WorldInfo>(id)`
- **THEN** 读回的 world 矩阵与写入一致
