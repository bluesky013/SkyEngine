## 1. core ECS 基础容器（core/ecs/）

- [x] 1.1 `core/ecs/EntityId.h`：`EntityId`（uint32，24 bit index + 8 bit generation）+ `MakeEntityId` / `GetIndex` / `GetGeneration` / `INVALID_ENTITY`
- [x] 1.2 `core/ecs/SparseSet.h`：`SparseSet<T>`（sparse 页表 + dense entity/data 双数组；`Add`/`Remove`(swap-remove)/`Get`(generation 校验)/`Contains`/dense 迭代/`DenseEntity(i)`）
- [x] 1.3 `core/ecs/EntityRegistry.h`：`CreateEntity`（free list + generation）/`DestroyEntity`/`Pool<T>()`（type id 懒建池）/`Add<T>`/`Get<T>`/`Remove<T>`；`TypeId<T>()` 静态自增
- [x] 1.4 `engine/test/core/EcsTest.cpp`（新增）：EntityId 位布局、generation 防悬垂、SparseSet add/remove/iterate、swap-remove 映射正确性、registry 生命周期

## 2. scene 迁入 aurora/core 并 ECS 化

- [x] 2.0 迁移 `engine/aurora/pipeline/{include/aurora/pipeline/scene, src/scene}` → `engine/aurora/core/{include/aurora/scene, src/scene}`（include 路径 `aurora/pipeline/scene/*` → `aurora/scene/*`）；`aurora/pipeline` CMake 链接 `Aurora` target；修正引用方 include（SceneRasterPassTemplate、测试）
- [x] 2.1 `aurora/core/include/aurora/scene/SceneTypes.h`：`Bounds`(AABB) / `RenderItem`(`{ Name techniqueTag; DrawItem item; }`，过渡形态) / `Light`(type/color/direction/intensity，占位) / `Skin`(占位)
- [x] 2.2 `RenderScene.h/.cpp` 重构：内嵌 `EntityRegistry`；`CreateEntity`/`DestroyEntity`/`Add<T>`/`Get<T>`/`Pool<T>`；views 保留独立 registry 不变
- [x] 2.3 删除 `RenderPrimitive.h` 与 `src/scene/RenderPrimitive.cpp`（`GatherContext`/`TechniqueBinding`/`GatherRenderItem` 一并删除）

## 3. Collect 适配

- [x] 3.1 `SceneRasterPassTemplate::Collect`：以 Bounds 池 dense 数组为主驱动遍历；`itemPool.Get(entity)` 查 RenderItem；queue.tag 非空时按 `RenderItem.techniqueTag` 过滤（空 tag 不过滤）；frustum cull + 排序语义不变

## 4. 测试适配

- [x] 4.1 `SceneCollectTest.cpp`：mock 由 `RenderPrimitive` 改为 `CreateEntity + Add<Bounds> + Add<RenderItem>`；断言行为不变（tag 过滤/frustum cull/空 tag/排序）

## 5. 验证与收尾

- [x] 5.1 全量 `cmake --build` 通过
- [x] 5.2 `AuroraTest` + `CoreTest` 全绿
- [ ] 5.3 `openspec archive aurora-scene-ecs` 归档本 change
