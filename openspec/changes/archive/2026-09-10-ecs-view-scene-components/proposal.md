## Why

ECS 容器已落地（`core/ecs`：EntityId / SparseSet / EntityRegistry / TypeId tag），但有两个缺口：

1. **场景组件太薄**：`Light` 只有 type/color/direction/intensity，point/spot 的 range、锥角等常用参数缺失；没有  `WorldInfo` 组件（实体没有位置/旋转/缩放，Bounds 无法由 transform 驱动）。
2. **没有 View**：多组件联合访问（如「有 Light 且有 WorldInfo 的实体」「Bounds + Light」）现在只能手写 `poolA` 遍历 + `poolB.Get` 逐个查，啰嗦且容易写错。ECS 惯例是提供一个 View 结构做组件交集迭代。

## What Changes

### Part 1 — 场景组件扩充（`aurora/core/scene/SceneTypes.h`）

- **扩充** `Light`：point/spot 参数 —— `position`（point/spot）、`range`（point/spot 衰减半径）、`innerConeAngle` / `outerConeAngle`（spot，弧度）；directional 只用 color/direction/intensity。
- **新增** `WorldInfo`：纯 world 矩阵（`Matrix4 world`；不拆 TRS，TRS 组合是上层的事）。
- 两个类型打 `SKY_TYPE_TAG`（`sky.aurora.Light` 已打，值不变；新增 `sky.aurora.WorldInfo`）。

### Part 2 — ECS View（`core/ecs/View.h`）

- **新增** `EntityRegistry::View<Ts...>()`：返回多池交集视图；迭代以**最小池为主驱动**（dense 扫），其余池 `Contains` 检查，命中回调 `fn(EntityId, Ts&...)`。
- **新增** `ForEach(fn)` 与基于迭代器的范围 for；单组件 View 退化为直接扫池。

## Capabilities

### New Capabilities

- `core-ecs-view`: `EntityRegistry::View<Ts...>` 的契约（最小组件池驱动、交集语义、引用回传）。

### Modified Capabilities

- `aurora-scene-collect`: `Light` 扩充 point/spot 参数；新增  `WorldInfo` 组件；`SceneRasterPassTemplate::Collect` 改用 `View<Bounds>`（cull-only 现状不变）。

## Impact

- **core**：新增 `core/ecs/View.h`；`EntityRegistry` 加 `View<Ts...>()`；`EcsTest.cpp` 加 View 测试。
- **aurora core**：`SceneTypes.h` 扩充/新增组件；tag 注册。
- **测试**：View 交集（两池/三池/空交集）、Light point/spot 参数、WorldInfo 矩阵存储。
- **不影响**：RDG、Collect 语义（仍 cull-only）、queue。
