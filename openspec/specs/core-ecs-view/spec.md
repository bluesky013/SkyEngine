# core-ecs-view Specification

## Purpose
TBD - created by archiving change ecs-view-scene-components. Update Purpose after archive.
## Requirements
### Requirement: ECS View 多池交集迭代

`EntityRegistry` SHALL 提供 `View<Ts...>()`，返回多组件池交集视图。迭代 SHALL 以最小组件池为主驱动（dense 连续扫描），其余池以 `Contains` 检查交集，命中回调 `fn(EntityId, Ts&...)`（各组件引用）。

#### Scenario: 两池交集
- **WHEN** entity e1 有 A+B，e2 只有 A，`View<A,B>().ForEach(fn)`
- **THEN** fn 只对 e1 触发，参数为 e1 的 A、B 引用

#### Scenario: 最小池驱动
- **WHEN** A 池 1000 项、B 池 3 项，`View<A,B>()`
- **THEN** 迭代扫描 B 池 dense 数组（3 次 Contains 检查），不扫 A 池

#### Scenario: 单组件 View
- **WHEN** `View<A>()`
- **THEN** 等价于直接扫 A 池 dense 数组

