## 1. ECS View（core/ecs/View.h）

- [x] 1.1 `EcsView<Ts...>`：持有各池引用；`ForEach(fn)` 以最小池为主驱动 dense 扫，其余池 `Contains` 交集检查，回调 `fn(EntityId, Ts&...)`
- [x] 1.2 `EntityRegistry::View<Ts...>()` 工厂；单组件 View 退化为直接扫池
- [x] 1.3 `EcsTest.cpp` 新增 View 测试：两池交集 / 最小池驱动 / 三池 / 空交集 / 单组件

## 2. 场景组件扩充（aurora/core/scene/SceneTypes.h）

- [x] 2.1 `Light` 扩充：`position` / `range` / `innerConeAngle` / `outerConeAngle`（point/spot 参数）
- [x] 2.2  `WorldInfo` 新增：`Matrix4 world`（默认 Identity）
- [x] 2.3 `SKY_TYPE_TAG(sky::aurora::WorldInfo, "sky.aurora.WorldInfo")`（Light 已有 tag 不变）

## 3. Collect 适配

- [x] 3.1 `SceneRasterPassTemplate::Collect` 改用 `scene.View<Bounds>()`（cull-only 行为不变）

## 4. 测试

- [x] 4.1 Light point/spot 参数读写断言
- [x] 4.2 WorldInfo 矩阵存储断言（写入/读回一致）
- [x] 4.3 SceneCollectTest 回归（Collect 走 View<Bounds>，行为不变）

## 5. 验证与收尾

- [x] 5.1 全量 `cmake --build` 通过
- [x] 5.2 `CoreTest` + `AuroraTest` 全绿
- [ ] 5.3 待用户确认后 `openspec archive ecs-view-scene-components` 归档本 change
