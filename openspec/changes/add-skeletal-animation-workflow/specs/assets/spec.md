## ADDED Requirements

### Requirement: Skeletal product saves report checked publication outcomes

The skeletal import path SHALL build its complete required Mesh/Skin/Skeleton product list before calling the existing Framework SaveProducts API. Encoding, staging, flush, readback, per-file atomic replacement and index failures SHALL report explicit failure. A failed batch SHALL NOT emit cook success or be accepted as successful package output. No persistent product group or rollback to a previous complete group is required; storage follows simplify-asset-product-storage.

#### Scenario: Failure after replacing a mesh
- **WHEN** a later Skin/Skeleton replacement or index update fails
- **THEN** the batch reports failure and its affected output is invalidated rather than accepted as a successful partial cook

### Requirement: Immutable loaded instances during binding and reload

Skeletal products SHALL load through the existing UUID product loader using in-memory asset instance identities/revisions. Reload SHALL create new immutable loaded objects rather than mutate published data or reopen retained bulk by UUID path. Mesh/Skin/Skeleton candidates SHALL validate their complete slot/layout/reference-pose combination and fixed loaded instances before switching. Existing consumers and submitted GPU work SHALL retain active data through normal retirement. Runtime and packaging SHALL use checked product data without source access or runtime group metadata.

#### Scenario: Rebuild while a candidate is being prepared
- **WHEN** a product change invalidates a skeletal binding candidate
- **THEN** the resolver rebuilds and validates the complete candidate before replacing the active binding
- **AND** it does not mutate already-submitted frame resources

#### Scenario: Products-only package
- **WHEN** a World referencing separate Mesh/Skin/Skeleton assets and a Graph is packaged without sources
- **THEN** explicit component roots and transitive dependencies resolve through checked UUID products without group manifests

### Requirement: Generated child cook requests complete through their owner

A missing generated child with a valid catalog owner SHALL request the owner source cook, coalesced by owner and target bundle. Every child receives success only when its own requested checked product exists after successful cook. Failure or a removed child SHALL complete as Failed with a diagnostic. No child SHALL recursively schedule a nonexistent child source or remain Loading indefinitely.

#### Scenario: Concurrent skin and skeleton requests
- **WHEN** an uncooked Skin and Skeleton from one model are requested together
- **THEN** one owner cook per bundle serves both requests and both reach terminal results

#### Scenario: Requested child removed by reimport
- **WHEN** the owner cooks successfully but a requested child no longer exists
- **THEN** that child fails explicitly instead of inheriting owner success

### Requirement: Persistent generated subproduct identities

A source manifest entry SHALL support generated subproduct records with stable source-local keys, UUIDs and asset types, plus optional default component-binding metadata. Skin and Skeleton identities SHALL be assigned once and reused across recook, source moves and bundles. Copying a source SHALL allocate new child identities. Keys SHALL use persistent source identities when available and otherwise explicit canonical paths; ambiguous renames SHALL NOT silently reassign an existing UUID. Removed children SHALL be reported as removed and their UUIDs SHALL NOT be reused for unrelated products.

#### Scenario: Recook and move preserve children
- **WHEN** a model retaining its source identity is recooked or moved
- **THEN** matching child keys SHALL retain their Skin/Skeleton UUIDs and the default component combination SHALL reference those identities

#### Scenario: Duplicate source has independent bindings
- **WHEN** a source is copied through the asset mutation service
- **THEN** the copied source SHALL receive distinct source and child identities without overwriting the original manifest records

#### Scenario: Source-local identity is ambiguous
- **WHEN** a model revision cannot unambiguously match a previous child key
- **THEN** import SHALL report the affected identity and require explicit rebinding instead of guessing from a similar bone or mesh name

### Requirement: Generated catalog entries resolve to their owner

The source catalog SHALL expose generated child UUID/type/owner records as metadata for selection, dependency validation and cook routing. Generated children SHALL NOT require fictitious source files or a second database. Dependency registration on a child SHALL retain the child product UUID while tracking its real owner source for recook. Runtime SHALL obtain child data exclusively from products. Import default mesh/skin/skeleton combinations SHALL initialize explicit component references and SHALL NOT become Mesh-owned Skin/Skeleton dependencies.

#### Scenario: Graph references an imported skeleton
- **WHEN** a Graph builder validates a Skeleton UUID generated by a model import
- **THEN** catalog lookup SHALL resolve the child's type and owner source without loading Aurora Adaptor or reading a runtime skeleton

#### Scenario: User selects an imported skin
- **WHEN** the reflected asset picker is filtered to AuroraSkin
- **THEN** valid generated Skin entries SHALL be selectable and SHALL identify their owner source for reimport

## MODIFIED Requirements

### Requirement: 动画资产（AnimationClip / Animation graph）

Content Adaptor SHALL 保留 AnimationClipAssetData、AssetTraits<AnimationClip>、Clip handler、反射和 CreateAnimationClipFromAsset。Clip 产品 SHALL 保存版本、name/frameRate、权威 sourceFrameCount、Skeleton UUID、通道 header/bulk，并校验格式与通道。ContentReflection SHALL 只注册其拥有的 Clip 资产，不再注册空 AnimationAssetData 或 AnimationGraph handler。Graph 的 schema/handler/反射 SHALL 唯一归 engine/animation 的 Animation.GraphAssets；原资产类型字符串 AnimationGraph 与源 UUID SHALL 保持。

#### Scenario: 动画资产往返
- **WHEN** 一个有效新版本 Clip 产品完成 Save/Load
- **THEN** name/frameRate/sourceFrameCount/Skeleton UUID 与通道数据 SHALL 保持一致，压缩 SHALL 不改变源时长

#### Scenario: 版本不匹配
- **WHEN** 加载不支持的 Clip 产品版本
- **THEN** loader SHALL 返回格式/recook 诊断，SHALL NOT 按新布局读取旧 payload

#### Scenario: Graph owner 唯一
- **WHEN** Content.Module 与 Animation.World 加载
- **THEN** Content SHALL 仅提供 Clip handler，AnimationGraph SHALL 由 Animation.GraphAssets 唯一注册

### Requirement: Animation builder（.clip / .graph）

Content 的 AnimationBuilder SHALL 仅声明 .clip，QueryType SHALL 返回 AnimationClip 资产类型。Request SHALL 读取支持的 Clip 源版本，验证压缩前长度/原始通道、压缩后保持 sourceFrameCount，并登记 Skeleton 产品 UUID 与其真实 owner source 依赖。产品发布 SHALL 使用显式结果，失败 SHALL 不被 FinishProduct 报告为成功。Content.Builder.Module SHALL 注册 Clip builder；.graph SHALL 唯一由 Animation.Builder 提供。Content.Module SHALL 不注册任何 builder。

#### Scenario: .clip 产出 AnimationClip
- **WHEN** 构建有效 .clip 源
- **THEN** SHALL 输出保持原始长度的压缩 Clip 并登记 Skeleton 依赖，包括 generated Skeleton 的 owner source

#### Scenario: 注册 builder
- **WHEN** Content.Builder.Module 初始化但 Animation.Builder 未加载
- **THEN** .clip SHALL 可构建，Content SHALL 不提供 .graph 的占位成功路径

#### Scenario: Graph plugin 接管
- **WHEN** Animation.Builder 随工具模块加载
- **THEN** .graph SHALL 由其唯一处理，卸载或缺失时 SHALL 明确不可用而非退回旧空 Graph

### Requirement: Content 模块与 adaptor / builder 分层

Content SHALL retain its existing adaptor, offline builder and runtime targets. Content.Adaptor SHALL provide Clip schema/conversion and the Clip source implementation using Animation and Framework interfaces. Content.Metadata SHALL own ContentReflection and persistent Clip handlers once per process definition. Content.Builder.Module SHALL depend on metadata and register Clip builders through RegisterMeta; it SHALL NOT depend on the runtime service. Content.Module SHALL depend on metadata and publish/revoke IAnimationClipSource in Init/Shutdown without registering reflection again. Graph schema/handlers SHALL belong to Animation.GraphAssets and Graph builders SHALL belong to Animation.Builder under engine/animation. Animation core SHALL remain independent of rendering and Framework implementation.

#### Scenario: Offline cook without runtime service
- **WHEN** Content.Metadata and Content.Builder.Module load without Content.Module
- **THEN** Clip schema, handler and builder are available without starting a runtime Clip provider

#### Scenario: Runtime session restart
- **WHEN** Content.Module shuts down and initializes again
- **THEN** its service is revoked and republished while the same metadata definition remains registered
