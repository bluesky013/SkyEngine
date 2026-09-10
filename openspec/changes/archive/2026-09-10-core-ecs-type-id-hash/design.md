## Context

`EntityRegistry` 用 `TypeId<T>()`（函数内 static + 全局自增计数器）索引组件池。本仓库 plugins 与 aurora 后端是 SHARED DLL 且静态链接 Core——每个 DLL 有独立的 Core 静态副本，同一类型跨模块 id 不一致。

## Goals / Non-Goals

**Goals:**

- `TypeId<T>()` 改为**纯显式 tag**（单层）：`SKY_TYPE_TAG(T, "sky.xxx.Type")` 宏特化 `TypeTagOf<T>`，哈希纯字符串——平台 / 编译器 / 持久化全稳定（字符串是唯一事实源，跨 toolchain 一致，可写入资产）。
- **无 tag 即编译失败**：未打 tag 的类型 `static_assert` 报错，不提供任何 fallback（杜绝隐式不稳定 id）。
- 场景组件（Bounds / RenderItem / Light / Skin）等引擎定义类型全部打显式 tag。
- debug 下哈希碰撞检测（tag 字符串比对断言）。

**Non-Goals:**

- 不做任何隐式/签名 fallback（`__FUNCSIG__` 方案废弃——不允许退化路径）。
- 不改 `SparseSet` / `EntityRegistry` 的池接口（`Pool<T>()` 签名不变）。
- 不引入 RTTI 依赖。

## Decisions

### 1. 纯显式 tag TypeId

```cpp
// 显式 tag：平台/编译器/持久化全稳定
template <typename T> struct TypeTagOf;  // 默认不定义 value

constexpr uint32_t Fnv1a32(std::string_view sv) { /* 编译期迭代 */ }

template <typename T>
constexpr uint32_t TypeId()
{
    static_assert(requires { TypeTagOf<T>::value; },
        "TypeId<T>: type has no SKY_TYPE_TAG; register an explicit tag");
    return Fnv1a32(TypeTagOf<T>::value);
}

// 注册宏（组件声明处一行）
#define SKY_TYPE_TAG(T, STR) \
    template <> struct ::sky::TypeTagOf<T> { static constexpr std::string_view value = STR; }
```

- 无 static：纯 constexpr 计算，所有模块算出同值。
- 跨 toolchain 一致（字符串与编译器无关）→ **可持久化**（资产写 "sky.aurora.Light"，加载时映射回）。
- 无初始化顺序 / 调用顺序依赖。
- 与 framework 既有 `TypeInfoObj<T>` 特化惯例一致。
- 未注册类型编译期即报错——不存在隐式不稳定路径。

### 2. 碰撞检测（debug）

`PoolHolder<T>` 记录 tag 字符串；`Pool<T>()` 在 debug 下发现「hash 命中已有池但 tag 不同」→ `SKY_ASSERT` 失败。32 位 FNV 在引擎组件数量级（数百）下碰撞概率可忽略，碰撞检测是兜底。

### 3. tag 字符串规范

`"sky.<module>.<TypeName>"` 格式（如 `sky.aurora.Light` / `sky.core.Bounds`），模块级前缀防冲突。持久化格式与运行时 id 都用同一字符串。

## Risks / Trade-offs

- **[tag 字符串冲突]** 两个不同类型打同一 tag 字符串。→ 模块前缀规范 + debug 碰撞检测。
- **[仪式成本]** 每个 ECS 类型必须写一行 `SKY_TYPE_TAG`，无例外。→ 与 framework `TypeInfoObj` 模式一致；编译期报错保证不会遗漏。

## Migration Plan

1. 新增 `core/ecs/TypeId.h`：`TypeSignature<T>()` + `Fnv1a32` + `TypeTagOf<T>` + `TypeId<T>()`（两层 constexpr）+ `SKY_TYPE_TAG` 宏。
2. `EntityRegistry.h`：删 `detail::NextTypeId` 与 static 版 `TypeId<T>()`；include `TypeId.h`；`PoolHolder` 加签名记录 + debug 碰撞断言。
3. `SceneTypes.h`：Bounds / RenderItem / Light / Skin 打 `SKY_TYPE_TAG`。
4. `EcsTest.cpp`：同类型同 id、不同类型异 id、顺序无关、显式 tag 生效、碰撞检测路径。
5. 全量编译 + 测试。
6. archive。
