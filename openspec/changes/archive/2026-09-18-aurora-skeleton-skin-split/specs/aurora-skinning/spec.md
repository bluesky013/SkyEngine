## ADDED Requirements

### Requirement: aurora 不定义 Skeleton

Aurora SHALL NOT 定义 `Skeleton` / `Bone` 类型，SHALL NOT 新增 `aurora/animation` 目录或模块。动画 rig 属于独立模块 `engine/animation`（target `Animation`，仅依赖 `Core`）。`aurora/core` SHALL NOT 依赖 `Animation`。

#### Scenario: 无 aurora Skeleton

- **WHEN** 检索 `sky::aurora` 命名空间
- **THEN** 不存在 `Skeleton` / `Bone` 类型，也不存在 `aurora/animation/` 头文件

### Requirement: Skin 自持 mesh 侧蒙皮绑定

`Skin`（`aurora/resource/Skin.h`）SHALL 自持 mesh 侧蒙皮数据：`inverseBindMatrices`（bind pose）、`boneMatrices`（求值后的调色板）与可选 `boneMapping`（顶点骨槽 → 骨索引）。`Skin` SHALL NOT 引用任何动画模块类型。

#### Scenario: 设置绑定数据

- **WHEN** `SetInverseBindMatrices` / `SetBoneMatrices` / `SetBoneMapping` 后查询
- **THEN** 对应 getter 返回相同规模的数据

#### Scenario: 不依赖动画类型

- **WHEN** 编译 `aurora/core`
- **THEN** 不需要 `Animation` 模块，`Skin` 头文件不含动画类型

### Requirement: 场景组件为 SkinnedMesh

`aurora/scene/SceneTypes.h` SHALL 定义场景组件 `SkinnedMesh`（持 `CounterPtr<Skin>`），其 `SKY_TYPE_TAG` SHALL 为 `sky.aurora.SkinnedMesh`。SHALL NOT 使用 `Skin` 作为场景组件名，以免与 mesh 侧 `Skin` 冲突。

#### Scenario: 注册 SkinnedMesh 组件

- **WHEN** 对实体 `Add<SkinnedMesh>` 后查询
- **THEN** `Get<SkinnedMesh>` 返回非空，实体销毁后按代际拒绝

### Requirement: Mesh 只暴露皮肤接口

`Mesh` SHALL 暴露 `SetSkin/GetSkin/HasSkin`。`HasSkin()` SHALL 反映是否绑定 `Skin`。`Mesh` SHALL NOT 暴露 `SetSkeleton/GetSkeleton/HasSkeleton`（动画 rig 通过桥接层映射进 `Skin`）。

#### Scenario: 设置皮肤

- **WHEN** `SetSkin(skin)` 后
- **THEN** `HasSkin() == true`，且不存在骨架相关 API

#### Scenario: 未设置皮肤

- **WHEN** 新建 Mesh
- **THEN** `HasSkin() == false`
