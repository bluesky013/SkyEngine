## MODIFIED Requirements

### Requirement: EntityRegistry

`EntityRegistry` SHALL 提供 `CreateEntity`（free list 复用 index，generation 自增）/ `DestroyEntity` / 按组件类型懒建 `SparseSet<T>` 池 / `Add<T>` / `Get<T>` / `Remove<T>`。

组件池索引 SHALL 使用 `TypeId<T>()`：**显式 tag 字符串的 constexpr 哈希**（`SKY_TYPE_TAG(T, "sky.<module>.<TypeName>")` 特化 `TypeTagOf<T>`，FNV-1a over tag 字符串）。SHALL NOT 使用函数内 static、全局计数器或任何签名 fallback；未注册类型 SHALL 编译失败（`static_assert`）。

#### Scenario: entity 生命周期
- **WHEN** CreateEntity → Add 组件 → DestroyEntity
- **THEN** 组件池对应项被移除；再次 CreateEntity 复用 index 且 generation 不同

#### Scenario: 跨平台/编译器/模块类型 id 一致
- **WHEN** 同一类型在不同模块（exe / plugin DLL / backend DLL）或不同 toolchain 调用 `TypeId<T>()`
- **THEN** 返回值一致（tag 字符串与平台/编译器无关 → 哈希一致）

#### Scenario: id 与调用顺序无关
- **WHEN** 两个模块以不同顺序首次调用 `TypeId<A>()` / `TypeId<B>()`
- **THEN** `TypeId<A>()` 在两模块中返回相同值（无首次调用序依赖）

#### Scenario: 未注册类型编译失败
- **WHEN** 对未打 `SKY_TYPE_TAG` 的类型调用 `TypeId<T>()` 或 `Pool<T>()`
- **THEN** 编译期 `static_assert` 失败（无隐式 fallback）

#### Scenario: 哈希碰撞检测
- **WHEN** debug 构建下 `Pool<T>()` 发现 hash 命中已有池但 tag 字符串不同
- **THEN** 断言失败（碰撞兜底）
