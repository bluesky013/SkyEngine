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

### Requirement: mesh 源导入

Cook SHALL 通过现有 Assimp importer 读取受支持的 gltf/glb/fbx/obj 数据，保留原始节点身份、每 mesh-binding 的 offset matrices、rig 关联和 influence。蒙皮导入 SHALL 在限制权重或优化节点/mesh 之前验证原始输入，SHALL NOT 用 LimitBoneWeights 静默截断或以全局骨名表吞掉不同绑定。转换空间与共享判定 SHALL 遵守 aurora-cook；不能可靠恢复的绑定关联 SHALL 返回 Unsupported。静态三角化、法线/UV/切线生成仍使用现有管线。

#### Scenario: OBJ quad 导入
- **WHEN** 导入有效的带 pos/uv/normal 的 OBJ quad
- **THEN** SHALL 产出三角形 primitive、有效属性与材质槽

#### Scenario: 无效源拒绝
- **WHEN** 导入无法解析的字节或无法区分的骨架绑定
- **THEN** SHALL 返回具体失败，不发布错误或不完整产品

#### Scenario: 原始权重超限
- **WHEN** 原始顶点包含超过四个不同的正权重骨骼
- **THEN** SHALL 在任何裁剪前报错，不把 Assimp 已裁剪结果当作合法原始输入

### Requirement: 交错顶点流组装

Cook SHALL 按 POSITION/NORMAL/TANGENT/UV1/COLOR/JOINTS/WEIGHTS 的既有顺序与格式组装每个可合并几何组，JOINTS 使用 U_RGBA8 槽索引。位置 SHALL 烘焙 mesh 节点变换，法线 SHALL 使用 inverse-transpose，切线 SHALL 使用线性变换再相对变换后的法线正交化；镜像变换 SHALL 同步修正切线 handedness 与三角形绕序。index 类型 SHALL 按顶点数选择 U16/U32，几何 bounds 与 mesh-space 槽 bounds SHALL 从最终顶点流构造。

#### Scenario: 属性表与 stride
- **WHEN** 组装含 pos/normal/tangent/uv 的源
- **THEN** 属性 SHALL 紧凑排列且 stride 为 48，vertex/index 尺寸与计数一致

#### Scenario: tangent 可关
- **WHEN** 配置 tangents=false
- **THEN** 属性表 SHALL 不含 TANGENT

#### Scenario: 非均匀与镜像变换
- **WHEN** mesh 节点有可逆非均匀缩放或镜像
- **THEN** 烘焙后的法线/切线 SHALL 正交且绕序/handedness 与变换一致

### Requirement: Mesh 几何产品与独立 Skin 产品

新版 MeshAssetData SHALL 保存版本、属性/计数/stride/indexType、材质引用、几何 bulk、输入槽布局/模型空间契约与槽 bounds；SHALL 移除 v5 skin bulk 和 Skeleton 引用，不恢复旧 v2 skin UUID 或已删除 meshlet 字段。Skin SHALL 作为版本化 AuroraSkin 子产品，组件组合与稳定身份 SHALL 遵守 aurora-cook 和 asset-source-identity。旧产品 SHALL 返回 recook 诊断。

#### Scenario: 新几何产品往返
- **WHEN** 新版 Mesh 产品完成 Save/Load
- **THEN** 几何、材质与输入槽契约 SHALL 保持一致，SHALL 不包含选定 Skin/Skeleton

#### Scenario: 旧产品拒绝
- **WHEN** 加载旧 v2 或 v5 Mesh 产品
- **THEN** SHALL 返回版本/recook 诊断，不按新布局解析

#### Scenario: 蒙皮产品组合
- **WHEN** cook 有效蒙皮模型且 skinning=true
- **THEN** SHALL 发布独立 Mesh/Skin/Skeleton 产品，默认组合存入导入元数据而非 Mesh 依赖表
