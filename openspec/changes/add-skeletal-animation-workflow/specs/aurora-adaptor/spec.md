## ADDED Requirements

### Requirement: Actor skeletal mesh component
Aurora Adaptor SHALL provide a reflected SkeletalMeshComponent with independent mesh, skin and skeleton UUIDs, poseSource ReferencePose or SameActorAnimation, material-slot overrides, visibility and shadow settings. New components SHALL default to ReferencePose. Persisted data SHALL contain identities/configuration without RHI objects or animation plugin implementation types. Import defaults SHALL be copied into explicit component fields. ReferencePose SHALL use the selected skeleton without requiring an animation plugin; SameActorAnimation SHALL require matching skeleton identity/revision/layout and report missing or invalid providers rather than claim animation success.

#### Scenario: Component serialization and reference pose
- **WHEN** an actor containing only a valid SkeletalMeshComponent is saved, reopened and rendered
- **THEN** its asset bindings and overrides SHALL persist and it SHALL display its reference pose

#### Scenario: Shared mesh with two skeleton definitions
- **WHEN** two actors select the same Mesh but different valid Skin/Skeleton combinations
- **THEN** each component SHALL retain its own combination through save/reopen and neither SHALL mutate shared geometry

### Requirement: Consumer-owned skeletal pose source
Aurora SHALL define the renderer-neutral skeletal pose source interface consumed by its scene bridge. An external provider SHALL publish owning CPU matrices, rig identity/layout signature, generation, revision and status for the current World/Actor. Aurora Core SHALL NOT depend on Animation or on the animation plugin; Aurora Adaptor SHALL NOT include/link the plugin implementation to discover components.

#### Scenario: Animation plugin absent
- **WHEN** no pose provider is registered
- **THEN** static scene behavior SHALL remain unchanged and a valid skeletal mesh SHALL use its declared reference-pose/error policy without loading a plugin implicitly

### Requirement: Atomic skeletal scene snapshots
RenderSceneBridge SHALL append skeletal entities through the sole producer and owned FramePacket after CPU completion. Mesh/Skin/rig versions, resolved binding generation, transform, palette, Pose revision and bounds SHALL be coherent. The animation system SHALL NOT load Mesh/Skin to coordinate rendering; the Aurora binding SHALL validate candidates and retain the last complete render snapshot on same-binding reload failure, with an observable stale revision. Busy retry SHALL preserve the original packet; stale generations or mismatched signatures SHALL be rejected.

#### Scenario: Busy while animation continues
- **WHEN** a packet containing pose N is rejected as Busy while the World computes pose N+1
- **THEN** retrying the original packet SHALL still submit pose N with its original transform and bounds

#### Scenario: Stale pose after actor recreation
- **WHEN** an actor identity is recreated with a new generation and an older pose completes
- **THEN** the old pose SHALL NOT update the new render entity

#### Scenario: Rig update precedes skin update
- **WHEN** the animation provider publishes a new rig revision while the matching Skin binding is still loading or fails validation
- **THEN** the bridge SHALL retain an explicitly marked old complete snapshot or report Error, and SHALL NOT mix the new Pose with the old binding

### Requirement: Skeletal asset mismatch is explicit
The bridge SHALL validate the Mesh input-slot/model-space contract against the selected Skin, resolve Skin targets against the selected skeleton, and verify finite invertible IB and G_bind[r(s)] * IB[s] approximately identity per effective slot. Named mode SHALL reject missing/ambiguous canonical targets. Indexed mode SHALL additionally require the declared rig layout signature. Resolved mappings SHALL be immutable and keyed by exact Mesh/Skin/rig revisions; neither bone count nor matching short names alone SHALL prove compatibility.

#### Scenario: Mesh and animation rig differ
- **WHEN** a component's selected Skin or Pose is incompatible with its mesh-slot contract or skeleton
- **THEN** the bridge SHALL report the mismatch and SHALL NOT submit a partially compatible draw

#### Scenario: Indexed mapping after skeleton reorder
- **WHEN** an indexed Skin is paired with a reordered skeleton having the same bone count but a different signature
- **THEN** binding SHALL fail before rendering instead of applying in-range indices to different bones

#### Scenario: Named mapping with incompatible bind pose
- **WHEN** all target names resolve but the selected skeleton's reference transforms do not satisfy the Skin bind invariant
- **THEN** binding SHALL report the offending slot and SHALL NOT silently recalculate inverse binds or retarget the asset

### Requirement: Independent skin asset lifecycle
Aurora Adaptor SHALL register the versioned AuroraSkin handler and construct an immutable Skin independently of Mesh. Missing/invalid Skin SHALL produce an asset diagnostic. Runtime geometry resources SHALL NOT own per-instance Skin assignments or mutable palette data. The standard product loader and revision lifetimes SHALL be used, including unloaded animation-plugin operation in ReferencePose mode.

#### Scenario: Select another skin while the old one is in flight
- **WHEN** a component explicitly selects Skin B while frames using Skin A are submitted
- **THEN** B SHALL load and validate under a new binding generation, A SHALL remain valid for submitted frames, and other instances using A SHALL remain unchanged

## MODIFIED Requirements

### Requirement: Mesh 资产材质槽引用与版本
MeshAssetData SHALL 保留 version、materials UUID 槽表与 GetMaterialUuid(materialIndex)，子网格 materialIndex SHALL 按表解析。新产品 SHALL 序列化独立 Mesh 的几何及输入槽契约，SHALL NOT 序列化选定 Skin/Skeleton UUID 或 inverse bind bulk。版本不匹配 SHALL 显式拒绝，SHALL NOT 按新布局读取旧产品。材质槽行为 SHALL 保持兼容。

#### Scenario: 材质槽与版本往返
- **WHEN** 新版 MeshAssetData 带 materials 完成 Save/Load
- **THEN** version 和 materials 顺序 SHALL 一致

#### Scenario: 解析材质索引
- **WHEN** materialIndex 小于 materials.size()
- **THEN** GetMaterialUuid SHALL 返回对应槽 UUID

#### Scenario: 越界索引回退
- **WHEN** 非空槽表的 materialIndex 越界
- **THEN** SHALL 保持既有槽 0 回退并报告警告

#### Scenario: 空槽表
- **WHEN** materials 为空
- **THEN** 子网格 SHALL 解析为无绑定材质且不崩溃

#### Scenario: 实例绑定不进入 Mesh 产品
- **WHEN** 两个组件为相同 Mesh 选择不同 Skin/Skeleton 后保存资产和 World
- **THEN** 各绑定 SHALL 保存在对应组件，Mesh 产品 SHALL 无 Skin/Skeleton 引用

#### Scenario: 版本不匹配
- **WHEN** loader 读取旧 Mesh 产品版本
- **THEN** SHALL 返回 recook 诊断且不产生错误索引或材质资源
