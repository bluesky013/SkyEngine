# aurora-skinning Specification

## Purpose

Define immutable inverse-bind Skin resources, geometry-only Mesh data and instance-selected Skin/Skeleton combinations. Runtime rig types remain in Animation; Aurora adaptor owns the Skeleton asset bridge, while Aurora core stays independent of Animation.

## Requirements
### Requirement: aurora 不定义 Skeleton

Aurora SHALL NOT 定义 `Skeleton` / `Bone` 类型，SHALL NOT 新增 `aurora/animation` 目录或模块。动画 rig 属于独立模块 `engine/animation`（target `Animation`，仅依赖 `Core`）。`aurora/core` SHALL NOT 依赖 `Animation`。

#### Scenario: 无 aurora Skeleton

- **WHEN** 检索 `sky::aurora` 命名空间
- **THEN** 不存在 `Skeleton` / `Bone` 类型，也不存在 `aurora/animation/` 头文件

### Requirement: Skin 自持 mesh 侧蒙皮绑定
`Skin`（`aurora/resource/Skin.h`）SHALL 为独立、发布后不可变的绑定资源，包含有序输入槽契约、每槽 inverseBindMatrices 和目标骨的规范名称/路径或索引。它 SHALL 由 Aurora 导入/cook 生成独立 AuroraSkin 产品，SHALL NOT 内嵌在 Mesh bulk、包含必需 Skeleton UUID 或持有求值后 boneMatrices。slot -> joint 映射 SHALL 在组合 Skin 与选定骨架时解析并验证。Skin SHALL NOT 引用动画模块类型；palette SHALL 由实例/帧持有。

#### Scenario: 设置绑定数据
- **WHEN** 构建 Skin 的 inverseBindMatrices 与目标骨标识后查询
- **THEN** getter SHALL 返回对应数据，发布后的绑定 SHALL 不被实例播放修改

#### Scenario: 不依赖动画类型
- **WHEN** 编译 aurora/core
- **THEN** SHALL 不需要 Animation 模块，Skin 头文件 SHALL 不含动画类型

#### Scenario: 两实例共享 mesh
- **WHEN** 两个 renderer entity 使用同一 Skin 并播放不同姿势
- **THEN** 两者 SHALL 持有独立 palette 和 Pose revision，共享绑定 SHALL 保持不变

#### Scenario: 不同 mesh 共用 skeleton
- **WHEN** 两个实例选择同一 Skeleton 而不同 Mesh 需要不同 inverse bind matrices
- **THEN** 实例 SHALL 分别选择自己的 Skin，不因 Skeleton UUID 相同而复用错误绑定

#### Scenario: 同一 mesh 使用不同绑定
- **WHEN** 两个实例引用同一 Mesh，分别选择 Skin A / Skeleton A 与 Skin B / Skeleton B，且两组绑定通过验证
- **THEN** 两者 SHALL 共享几何并生成各自 palette，切换一方的 Skin SHALL NOT 修改 Mesh 或另一方的绑定

### Requirement: 场景组件为 SkinnedMesh
`aurora/scene/SceneTypes.h` SHALL 保留场景组件 `SkinnedMesh` 与 tag `sky.aurora.SkinnedMesh`，关联不可变 Skin 及实例姿势/调色板状态。它 SHALL 不与 Framework Actor 的 SkeletalMeshComponent 混用，也 SHALL 不使用 Skin 作为场景组件名。销毁后的旧 generation SHALL 被拒绝。

#### Scenario: 注册 SkinnedMesh 组件
- **WHEN** 对实体 Add<SkinnedMesh> 后查询并销毁该实体
- **THEN** 有效代际的 Get SHALL 返回数据，销毁后旧代际 SHALL 被拒绝且在途帧数据仍受 lease 保活

### Requirement: Mesh 与实例 Skin 绑定分离
Mesh SHALL 仅保存几何与 influence 顶点数据及其输入槽契约，SHALL NOT 持有 Skin / Skeleton 资源引用或暴露 SetSkin/GetSkin/HasSkin、SetSkeleton/GetSkeleton/HasSkeleton。是否具有 influence 数据 SHALL 由明确顶点布局能力表达。绑定 SHALL 由场景实例选择；导入默认组合 SHALL 保存在导入元数据并复制为组件的显式引用。

#### Scenario: 实例切换皮肤
- **WHEN** 一个实例从 Skin A 切换为 Skin B
- **THEN** 共享 Mesh 内容 SHALL 不变，实例 SHALL 重新验证完整组合后再提交新绑定

#### Scenario: 仅创建几何资源
- **WHEN** 创建带 JOINTS / WEIGHTS 的 Mesh 而未创建场景实例
- **THEN** Mesh SHALL 不隐式分配 Skin、选择 Skeleton 或创建 palette
