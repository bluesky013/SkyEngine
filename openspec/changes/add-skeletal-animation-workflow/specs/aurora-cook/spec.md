## ADDED Requirements

### Requirement: Stable skin and skeleton subproduct identity
Skin and Skeleton child identities SHALL follow the generated identity contract of asset-source-identity. Mesh/Skin/Skeleton publication SHALL use checked SaveProducts with a complete required product list. A failed batch SHALL report failure and invalidate affected output rather than publish cook success. No product-group manifest, history or complete-old-group rollback is required; the flat format is defined by simplify-asset-product-storage.

#### Scenario: Rebuild preserves bindings
- **WHEN** the same model is recooked or moved while keeping its source identity
- **THEN** its logical skin and skeleton UUIDs SHALL remain unchanged and revisions/signatures SHALL reflect actual content changes

#### Scenario: Failure during child publication
- **WHEN** validation or saving fails before the parent/child set is complete
- **THEN** the build SHALL report failure and the previous complete published set SHALL remain resolvable

### Requirement: Bounded valid skin influences
The first format SHALL support at most 256 rig joints including helpers, 256 Skin slots and four positive influences per vertex using U_RGBA8 slot indices. Raw source influences SHALL be validated before destructive preprocessing: finite/nonnegative weights, duplicate-joint aggregation, positive influence count, valid indices and nonzero total. aiProcess_LimitBoneWeights SHALL NOT silently reduce unsupported input; node/mesh optimization SHALL NOT remove binding identity before capture. Weights SHALL be normalized and zero-weight lanes SHALL use a valid slot index. Cook SHALL produce mesh-space slot bounds and revalidate optimized output. Unsupported inputs SHALL return diagnostics.

#### Scenario: Joint index exceeds the supported format
- **WHEN** an imported rig exceeds 256 joints or an influence references a missing joint
- **THEN** cook SHALL fail before uint8 packing and SHALL NOT publish a wrapped index

#### Scenario: Original source exceeds four weights
- **WHEN** a vertex has five distinct positive influences before Assimp postprocessing
- **THEN** cook SHALL reject it rather than accept a silently truncated four-influence result

#### Scenario: Valid normalized weights and bounds
- **WHEN** a mesh has supported finite positive influences
- **THEN** stored weights SHALL sum to one within tolerance and per-slot bounds SHALL contain their associated mesh-space vertices

### Requirement: Explicit legacy skin migration
The new Mesh product version SHALL remove v5 skin bulk and skeleton references. Old Mesh products and historical AuroraSkin products with incompatible schemas SHALL be rejected with a recook diagnostic. Migration SHALL preserve the model source UUID and allocate persistent Skin child identities. A prior random Skeleton UUID MAY be seeded only when old product metadata proves an unambiguous rig match; otherwise migration SHALL report the need for explicit rebinding.

#### Scenario: Recook a v5 model
- **WHEN** a v5 model with a verifiable unique existing skeleton is recooked
- **THEN** its Mesh UUID and verified Skeleton UUID SHALL be preserved, its Skin child UUID SHALL be persisted, and its products SHALL use the new independent layout

#### Scenario: Old child identity cannot be recovered
- **WHEN** prior products are missing or identify ambiguous rigs
- **THEN** migration SHALL report unresolved references rather than claiming old skeleton references were preserved

## MODIFIED Requirements

### Requirement: Skeleton builder（.skeleton）
`AuroraSkeletonBuilder : AssetBuilder` SHALL 声明扩展名 `.skeleton`，QueryType SHALL 返回 `AssetTraits<sky::Skeleton>::ASSET_TYPE`（`AuroraSkeleton`）。Request SHALL 解析版本化 JSON bones（name、parent、translation、scale、xyzw rotation）并产出逐骨层级与参考姿势。发布前 SHALL 校验数量、唯一身份、父索引/无环、TRS 有限且可用；错误 SHALL 返回诊断，不构建依赖断言兜底的运行骨架。产品 SHALL 携带布局签名。

#### Scenario: .skeleton 产出骨架
- **WHEN** 构建含有效多级 bones 的 .skeleton
- **THEN** 产物 boneData/name/parentIndex/refPos SHALL 对应源定义并具有可重现布局签名

#### Scenario: 无效骨架
- **WHEN** bones 含环、重复身份、非法父索引、缺失参考姿势或非有限数据
- **THEN** 构建 SHALL 失败并报告具体骨骼，不发布新成功产品

### Requirement: Mesh builder 产出 skeleton
AuroraMeshBuilder SHALL 在启用 skinning 且源有蒙皮时，生成相互独立的 Mesh、AuroraSkin 和 AuroraSkeleton 产品。Mesh SHALL 保存 JOINTS / WEIGHTS、输入槽契约与 mesh-space 槽 bounds，不保存 inverse bind 或 Skin / Skeleton UUID；导入元数据 SHALL 提供默认 mesh/skin/skeleton 组合。Skin SHALL 保存输入槽、inverse bind 和骨标识，不固定 Skeleton UUID。骨架 SHALL 保留祖先闭包 / helper，rig 索引与 skin 槽 SHALL 独立。每份原始绑定 SHALL 先转换到约定模型空间并验证，SHALL NOT 全局按骨名吞掉后续 offsetMatrix。非蒙皮源 SHALL 不生成 Skin / Skeleton 组合。

#### Scenario: 蒙皮网格引用 skeleton
- **WHEN** 构建有效蒙皮源
- **THEN** SHALL 生成独立 mesh/skin/skeleton 产品及稳定默认组合，组件可按组合写入三项显式 UUID

#### Scenario: 非蒙皮网格无 skeleton
- **WHEN** 构建非蒙皮源
- **THEN** Mesh SHALL 不包含 skin bulk 或骨架引用，导入元数据 SHALL 无蒙皮组合

#### Scenario: Helper 节点和节点空间
- **WHEN** mesh 节点与关节之间存在非单位变换或非 deform helper 祖先
- **THEN** 输出 SHALL 保留正确层级/空间，参考姿势蒙皮 SHALL 重现烘焙顶点

#### Scenario: Multiple bindings preserve shared geometry
- **WHEN** source bindings use identical geometry, weights, input slot contract and model space but different rigs/inverse binds
- **THEN** cook SHALL retain separate Skin/Skeleton combinations and MAY share the identical Mesh product; unrecoverable source binding associations SHALL fail explicitly
