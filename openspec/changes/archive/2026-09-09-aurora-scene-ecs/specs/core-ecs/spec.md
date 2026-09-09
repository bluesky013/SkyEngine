## ADDED Requirements

### Requirement: EntityId

`EntityId` SHALL 为 32 bit：`24 bit index | 8 bit generation`。generation 在 entity 销毁后自增；持旧 generation 的引用访问 SHALL 失败（防悬垂）。

#### Scenario: generation 防悬垂
- **WHEN** entity 销毁后其 index 被新 entity 复用
- **THEN** 旧 EntityId（旧 generation）`Get` 返回 nullptr / Contains 返回 false

### Requirement: SparseSet 容器

`SparseSet<T>` SHALL 提供：sparse 页表（id.index → dense index）+ dense 数组（entity 与 component 双数组对齐）；`Add` / `Remove`（swap-remove 保 dense 连续）/ `Get` / `Contains` / dense 连续迭代。T SHALL 可移动。

#### Scenario: dense 连续迭代
- **WHEN** 多次 Add/Remove 后遍历 dense data 数组
- **THEN** 数组无空洞，顺序扫描不遇指针跳转

#### Scenario: swap-remove 保持映射正确
- **WHEN** 删除中间元素
- **THEN** 末尾元素换入被删槽位，其 sparse 项同步更新，所有存活 entity `Get` 结果正确

### Requirement: EntityRegistry

`EntityRegistry` SHALL 提供 `CreateEntity`（free list 复用 index，generation 自增）/ `DestroyEntity` / 按 component type 懒建 `SparseSet<T>` 池 / `Add<T>` / `Get<T>` / `Remove<T>`。

#### Scenario: entity 生命周期
- **WHEN** CreateEntity → Add 组件 → DestroyEntity
- **THEN** 组件池对应项被移除；再次 CreateEntity 复用 index 且 generation 不同
