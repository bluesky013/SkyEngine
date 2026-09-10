## Context

`core/ecs` 容器与 aurora scene 组件骨架已落地。缺口：Light 参数太薄、无 WorldInfo、无多组件联合访问结构。

## Goals / Non-Goals

**Goals:**

- `Light` 扩充 point/spot 常用参数（position / range / innerConeAngle / outerConeAngle）。
- `WorldInfo` 组件（纯 world 矩阵）。
- `EntityRegistry::View<Ts...>()`：多池交集迭代（最小池驱动）。

**Non-Goals:**

- 不做 transform 层级（parent/child、场景图）；不拆 TRS——v1 直接存 world 矩阵。
- 不做 Bounds 由 WorldInfo 自动驱动（后续 system）。
- 不做 View 的编译期过滤排除（entt 的 exclude<>，后续按需）。
- 不做光照计算/上传（数据结构先行）。

## Decisions

### 1. Light 参数布局

```cpp
struct Light {
    LightType type      = LightType::DIRECTIONAL;
    Vector3   color     = {1.f, 1.f, 1.f};
    float     intensity = 1.f;

    Vector3   direction       = {0.f, -1.f, 0.f}; // directional / spot
    Vector3   position        = {};               // point / spot
    float     range           = 10.f;             // point / spot 衰减半径
    float     innerConeAngle  = 0.f;              // spot（弧度）
    float     outerConeAngle  = 0.785398f;        // spot（弧度，~45°）
};
```

point 用 position+range；spot 再加 direction+双锥角；directional 只用 direction。参数自包含（v1 不从 WorldInfo 取 position，避免隐式耦合；后续如需可让 system 同步）。

### 2. WorldInfo 为纯 world 矩阵

```cpp
struct WorldInfo {
    Matrix4 world = Matrix4::Identity();
};
```

不拆 TRS（用户拍板：直接使用 world 矩阵；TRS 分解/组合是上层的事，组件不承载）。无 parent/层级（v1）；矩阵供 Bounds 更新 / 常量上传。

### 3. View 以最小池为主驱动

```cpp
template <typename... Ts>
class EcsView {
public:
    template <typename Fn>
    void ForEach(Fn &&fn);  // fn(EntityId, Ts&...)
};

template <typename... Ts>
EcsView<Ts...> EntityRegistry::View();
```

实现：编译期选 `sizeof...(Ts)` 池中 `Size()` 最小的为主驱动；dense 扫；对每个 entity 用其余池 `Contains` 检查；命中则回调各池组件引用。单组件 View 直接扫该池。

范围 for：提供 `begin()/end()` 迭代器（惰性 intersection），或仅 ForEach（v1 先 ForEach，迭代器后续按需）。

**View API 与存储解耦**：调用方只见 `View<Ts...>().ForEach(...)`，不感知底层存储。存储演进路径：v1 sparse-set + 最小池驱动交集检查（本 change）；v2 对热组合（如 Bounds+WorldInfo）引入 owning group 对齐存储（两池 dense 前段同序同区间，lockstep 零检查）——调用方无感。

### 4. Collect 改用 View<Bounds>

`SceneRasterPassTemplate::Collect` 由手写 `scene.Pool<Bounds>()` 改为 `scene.View<Bounds>()`——行为不变（仍 cull-only），但验证 View 是真实消费方。

## Risks / Trade-offs

- **[View 每实体 N-1 次 Contains]** 主驱动是最小池已控制复杂度；N 很大时可加 archetype 分组（v2）。
- **[Light 自包含 position vs 复用 WorldInfo]** v1 自包含避免隐式耦合；代价是 point/spot 实体若同时挂 WorldInfo 需要手动同步。→ 后续 light system 决定。

## Migration Plan

1. core：`core/ecs/View.h`（EcsView + 最小池驱动 ForEach）；`EntityRegistry::View<Ts...>()`；View 测试。
2. scene：`Light` 扩充 +  `WorldInfo` 新增 + tag。
3. Collect 改 `View<Bounds>`；测试适配。
4. 全量编译 + 测试；archive。
