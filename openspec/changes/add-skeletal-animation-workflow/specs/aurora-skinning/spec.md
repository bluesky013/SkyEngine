## ADDED Requirements

### Requirement: Consistent model-space skinning
Skinning SHALL interpret JOINTS as Skin slot indices s, resolve each slot to joint r(s), and use model-space palette P_s = G_r(s) * IB_s. The bind pose SHALL reproduce cooked geometry within tolerance for every effective slot. World transform SHALL be applied exactly once. Joint attributes, normalized weights and palette ranges SHALL be validated before preparation. Rig indices SHALL NOT be substituted for slot indices implicitly.

#### Scenario: Nonidentity mesh transform
- **WHEN** a fixture has a transformed mesh node, a helper parent and a nonidentity Actor transform
- **THEN** bind and animated vertex results SHALL match the CPU reference without double-applying any transform

#### Scenario: Rig order differs from skin slot order
- **WHEN** the selected skeleton contains helper joints and its joint order differs from the skin slots
- **THEN** each vertex SHALL use the palette entry for its slot, resolved to the correct rig joint

### Requirement: Same deformation in color and shadow
The production Opaque and Shadow paths SHALL consume the same palette revision and deformation semantics, including normal/tangent handling. Preparation SHALL assemble the effective shader variant from independently owned Vertex, Material/Batch and Pipeline inputs. Skinning SHALL be a Vertex-domain selection for the same authored program, not a separate Skinned source path or material asset. Material values SHALL NOT override Vertex or Pipeline inputs. The selected variant's reflection SHALL determine its vertex attributes and actual Pass resource declaration. A material lacking the required deformation contract SHALL return Unsupported rather than draw the static bind pose silently.

#### Scenario: Animated shadow
- **WHEN** a known two-bone mesh animates with shadow casting enabled
- **THEN** color and shadow outputs SHALL correspond to the same animated pose

#### Scenario: Unsupported custom material
- **WHEN** a skinned mesh uses a material without compatible joint/palette inputs
- **THEN** preparation SHALL report Unsupported and SHALL NOT use a static shader as a hidden fallback

#### Scenario: Same material with different geometry and pipeline inputs
- **WHEN** one material is used by static and skinned draws or by Views with different Pipeline switch values in the same frame
- **THEN** preparation SHALL select each complete Vertex/Material/Pipeline combination independently, retain the exact program and palette for each draw, and SHALL NOT use material UUID or queue Tag alone as the variant identity

#### Scenario: Material cannot override geometry or pipeline switches
- **WHEN** authored material overrides attempt to enable or disable a Vertex-domain skinning/instance switch or a Pipeline-domain shadow switch
- **THEN** validation SHALL reject the cross-domain assignment explicitly before shader preparation

#### Scenario: Cooked variant resolves without source
- **WHEN** a valid three-domain combination is cooked and later requested without shader sources or an online compiler
- **THEN** runtime selection SHALL use the same normalized inputs and identity as cook and load the matching program; an uncooked or incompatible combination SHALL report a missing/incompatible product rather than substitute another variant

### Requirement: Frame-owned palette transport
Palettes SHALL use the existing frame resource/lease and completion protocol with declared Batch storage-buffer bindings. GPU work SHALL retain exact palette contents until retirement. Implementations SHALL NOT create a second uploader ring or mutate an in-flight palette when a newer pose arrives.

#### Scenario: Multiple frames in flight
- **WHEN** a character advances across two in-flight frames and is then deleted or reloaded
- **THEN** each frame SHALL use its own submitted pose and its storage SHALL remain valid until that frame retires

### Requirement: Pose-consistent conservative bounds
Mesh SHALL store mesh-space AABBs per Skin input slot covering every associated positive-weight vertex. Runtime SHALL transform each nonempty slot bound by its own palette P_s and union them, then apply the instance world transform. Bounds SHALL contain current deformed geometry and use the same Pose/binding revision as drawing; they SHALL NOT embed one selected Skin's inverse bind or use another binding's cached joint-local bounds.

#### Scenario: Animation outside bind bounds
- **WHEN** an animated vertex leaves the bind-pose AABB
- **THEN** the current bounds SHALL still contain it and culling SHALL NOT reject the object based on its obsolete bind bounds

#### Scenario: Different skins share geometry bounds
- **WHEN** two instances share a Mesh but use different valid Skin mappings and inverse binds
- **THEN** each instance SHALL derive conservative bounds with its own palette without changing the Mesh bounds

## MODIFIED Requirements

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

### Requirement: Mesh 与实例 Skin 绑定分离
Mesh SHALL 仅保存几何与 influence 顶点数据及其输入槽契约，SHALL NOT 持有 Skin / Skeleton 资源引用或暴露 SetSkin/GetSkin/HasSkin、SetSkeleton/GetSkeleton/HasSkeleton。是否具有 influence 数据 SHALL 由明确顶点布局能力表达。绑定 SHALL 由场景实例选择；导入默认组合 SHALL 保存在导入元数据并复制为组件的显式引用。

#### Scenario: 实例切换皮肤
- **WHEN** 一个实例从 Skin A 切换为 Skin B
- **THEN** 共享 Mesh 内容 SHALL 不变，实例 SHALL 重新验证完整组合后再提交新绑定

#### Scenario: 仅创建几何资源
- **WHEN** 创建带 JOINTS / WEIGHTS 的 Mesh 而未创建场景实例
- **THEN** Mesh SHALL 不隐式分配 Skin、选择 Skeleton 或创建 palette

### Requirement: 场景组件为 SkinnedMesh
`aurora/scene/SceneTypes.h` SHALL 保留场景组件 `SkinnedMesh` 与 tag `sky.aurora.SkinnedMesh`，关联不可变 Skin 及实例姿势/调色板状态。它 SHALL 不与 Framework Actor 的 SkeletalMeshComponent 混用，也 SHALL 不使用 Skin 作为场景组件名。销毁后的旧 generation SHALL 被拒绝。

#### Scenario: 注册 SkinnedMesh 组件
- **WHEN** 对实体 Add<SkinnedMesh> 后查询并销毁该实体
- **THEN** 有效代际的 Get SHALL 返回数据，销毁后旧代际 SHALL 被拒绝且在途帧数据仍受 lease 保活
