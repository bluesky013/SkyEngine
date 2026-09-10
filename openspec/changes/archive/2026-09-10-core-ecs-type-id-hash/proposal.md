## Why

`core-ecs` 落地的 `TypeId<T>()` 用函数内 static 变量（`static const uint32_t id = detail::NextTypeId();`）。在本仓库的模块模型下这是 bug：

- plugins（BulletPhysicsModule 等）与 aurora 后端（AuroraVulkan / AuroraDX12）全部是 **SHARED DLL**；
- 它们都**静态链接 Core** —— 每个 DLL 持有自己的一份 Core 静态副本；
- 因此 `TypeId<T>()` 的 static 计数器在每个模块各有一份，且**赋值取决于各模块内的首次调用顺序**——同一类型在不同模块得到不同 id。

ECS 的设计目标是「通用可复用」（framework / plugins / render 共享），一旦跨 DLL 边界访问同一 registry 的组件池，id 不一致直接导致查错池或查空。

## What Changes

- **BREAKING** `TypeId<T>()` 实现从「函数内 static + 全局自增计数器」改为 **纯显式 tag**：`SKY_TYPE_TAG(T, "sky.xxx.Type")` 宏特化 `TypeTagOf<T>`，constexpr FNV-1a over tag 字符串。纯值，无运行时状态，无初始化顺序问题；tag 字符串与平台/编译器无关 → 跨模块、跨 toolchain、可持久化全稳定。**未打 tag 的类型 `static_assert` 编译失败，无 fallback。**
- **新增** 哈希碰撞的 debug 检测：`EntityRegistry` 的 `PoolHolder` 记录 tag，debug 下 `Pool<T>()` 发现 hash 相同但 tag 不同即断言。
- **新增** 场景组件（Bounds/RenderItem/Light/Skin）打显式 tag。
- **删除** `detail::NextTypeId` 全局计数器；不引入签名哈希 fallback（`__FUNCSIG__` 方案废弃）。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `core-ecs`: `TypeId<T>()` 语义变更——从「进程内首次调用序决定的不稳定 id」改为「显式 tag 字符串的 constexpr 哈希」；跨平台/编译器/模块全稳定；未注册类型编译期报错（无 fallback）。

## Impact

- **受影响代码**：`engine/core/include/core/ecs/EntityRegistry.h`（拆出 `TypeId.h`）；`engine/aurora/core/include/aurora/scene/SceneTypes.h`（打 tag）；`engine/test/core/EcsTest.cpp`（新增稳定性/顺序无关/显式 tag/碰撞检测测试）。
- **现有调用方**：仅 `aurora/core` 的 `RenderScene`（静态库，同进程单实例），行为不变。
- **约束**：显式 tag 字符串格式 `sky.<module>.<TypeName>`；tag 字符串是持久化与运行时 id 的唯一事实源。
