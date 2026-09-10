## 1. 纯显式 tag TypeId

- [x] 1.1 新增 `core/ecs/TypeId.h`：`TypeTagOf<T>`（默认无 value）+ `Fnv1a32(std::string_view)` constexpr + `TypeId<T>()`（`static_assert` 无 tag 则编译失败）+ `SKY_TYPE_TAG` 宏
- [x] 1.2 `EntityRegistry.h`：删 `detail::NextTypeId` 与 static 版 `TypeId<T>()`；include `TypeId.h`；`PoolHolder<T>` 记录 tag 字符串，debug 下 `Pool<T>()` 碰撞断言（hash 同、tag 异 → SKY_ASSERT）
- [x] 1.3 `SceneTypes.h`：Bounds / RenderItem / Light / Skin 打 `SKY_TYPE_TAG`（`sky.aurora.Bounds` 等）

## 2. 测试

- [x] 2.1 `EcsTest.cpp`：显式 tag 生效（tag 字符串决定 id）；同类型同 id；不同类型异 id；id 与首次调用顺序无关；既有 ECS 测试改为先打 tag 再注册组件
- [x] 2.2 既有 scene 测试行为不变（回归）

## 3. 验证与收尾

- [x] 3.1 全量 `cmake --build` 通过
- [x] 3.2 `CoreTest` + `AuroraTest` 全绿
- [ ] 3.3 `openspec archive core-ecs-type-id-hash` 归档本 change
