# assets Specification

## Purpose

Define source identity, mutation, provenance, product loading, content data and derived-data caching.

## Requirements

### Requirement: Ordered mount list

The asset database SHALL expose its source mounts as an ordered list, each carrying a stable id, a
display name, and a writability flag, in the same order used to resolve logical paths (earlier mounts
shadow later ones).

#### Scenario: Mounts exposed in order
- **WHEN** the database's mounts are queried
- **THEN** it SHALL return the mounts in resolution order with id, display name, and writable flag

#### Scenario: Single source of order
- **WHEN** a mount is added or removed
- **THEN** both path resolution and the exposed mount list SHALL reflect the same order

### Requirement: Per-source owning mount

The asset database SHALL record each source's owning mount id at registration and SHALL expose it, so
consumers do not re-derive provenance by probing filesystems.

#### Scenario: Owning mount reported
- **WHEN** a source is registered from a given mount
- **THEN** its owning mount id SHALL be reported and SHALL match the mount that contains the file

#### Scenario: No filesystem probing by consumers
- **WHEN** a consumer needs a source's provenance
- **THEN** it SHALL obtain it from the recorded owning mount, not by testing each filesystem

### Requirement: Single-pass all-mount scan

The source rebuild scan SHALL iterate the mount list and perform one recursive discovery pass per
mount, testing builder-known extensions during the walk (not one full walk per extension).

#### Scenario: One walk per mount
- **WHEN** the source cache is rebuilt
- **THEN** each mount SHALL be walked once and files of builder-known extensions SHALL be registered

#### Scenario: Workspace shadowing
- **WHEN** the same logical path exists in the writable mount and a later read-only mount
- **THEN** the writable mount's source SHALL win

### Requirement: Read-only identity without writes

Sources discovered in a read-only mount SHALL receive stable identity derived from their logical path
and SHALL NOT cause any manifest write; writable mounts SHALL read and write their directory manifests
as before.

#### Scenario: Read-only not written
- **WHEN** a source is registered from a read-only mount
- **THEN** no manifest file SHALL be created or modified and its identity SHALL be stable across scans

#### Scenario: Writable keeps manifest identity
- **WHEN** a source is registered in the writable mount
- **THEN** its identity SHALL come from (and be persisted to) the directory `assets.jsonl`

### Requirement: Loading is always from cooked products

The editor and the runtime SHALL load asset data exclusively from cooked products through the same product-based loader (`AssetManager`) and the same product path-to-UUID index. Neither the editor nor the runtime SHALL load asset data from source files. The source catalog (`assets.jsonl` / `assets.db`) MUST NOT be used to obtain asset data; it MAY only be queried to resolve a path/id, check existence, or select a cook target — never for asset data.

#### Scenario: Editor loads products

- **WHEN** an editor consumer requests an asset
- **THEN** it SHALL load the cooked product payload through `AssetManager`, exactly as the runtime does

#### Scenario: Editor load reads no source

- **WHEN** the editor loads an asset
- **THEN** it SHALL read only cooked products and the product index, and MUST NOT read the source file or the source manifest for asset data

### Requirement: Product payloads reference assets by UUID only

A product payload that references another asset SHALL store a UUID and MUST NOT store a source-relative path, so the loader never consults the source catalog for data.

#### Scenario: Material payload uses UUID references

- **WHEN** a material product references a texture
- **THEN** the payload SHALL contain the texture's UUID, and loading SHALL NOT resolve a source path

### Requirement: Path resolution has an editor-only source fallback

When a load key is a path, the loader SHALL resolve it through the product index first; on a miss with the editor source catalog present, it SHALL resolve through the source catalog; if both miss, the asset SHALL be reported missing. The runtime SHALL resolve paths only through the product index.

#### Scenario: Built asset resolves via the product index

- **WHEN** a path load matches an entry in the product index
- **THEN** the loader SHALL resolve the UUID from the product index without consulting the source catalog

#### Scenario: Uncooked source resolves via the editor fallback

- **WHEN** a path load misses the product index but the editor source catalog can resolve the path
- **THEN** the loader SHALL obtain the UUID from the source catalog and proceed to the on-demand cook branch

#### Scenario: Runtime path miss reports missing

- **WHEN** the runtime path load misses the product index
- **THEN** the load SHALL fail, since the runtime has no source catalog

### Requirement: On-demand cook when a product is missing

When a load request finds no product, the loader SHALL consult source existence: if a source record exists it SHALL schedule one cook (coalesced per UUID) and return the asset in a **LOADING** state, becoming **LOADED** on cook success or **FAILED** on cook failure; if no source record exists it SHALL report the asset as missing. Source existence SHALL mean an `AssetDataBase` record for the UUID, not the presence of the `assets.db` file. In the runtime the source locator is empty, so a missing product SHALL be a hard error.

#### Scenario: Missing product with a source triggers a cook

- **WHEN** the loader finds no product but a source record exists
- **THEN** it SHALL schedule a cook and return the asset in a LOADING state

#### Scenario: Cook target from the effective cook configuration

- **WHEN** a cook is scheduled
- **THEN** the target bundle SHALL be chosen from the effective cook configuration (asset override × project preset × current platform), defaulting to the primary bundle

#### Scenario: Missing product without a source reports missing

- **WHEN** the loader finds no product and no source record exists
- **THEN** it SHALL report the asset as missing

#### Scenario: Runtime missing product errors

- **WHEN** the runtime loads an asset with no product
- **THEN** the load SHALL fail, and no cook SHALL be attempted

#### Scenario: Cook failure leaves the asset FAILED

- **WHEN** a scheduled cook fails or no builder is available for the source
- **THEN** the asset SHALL become FAILED (not remain LOADING), MUST NOT fall back to the source, and a later load SHALL re-attempt

#### Scenario: Dependencies trigger their own cooks

- **WHEN** an asset's product exists but a dependency's product is missing with an available source
- **THEN** the dependency's load SHALL schedule its own cook and the dependent asset SHALL wait for the dependency chain

### Requirement: Cook completion is reported as an event

Cook completion SHALL be reported through an asset event keyed by the asset UUID, carrying success or failure, so a LOADING asset can transition and the caller can react. Both cook modes SHALL raise the same event.

#### Scenario: Success completes the pending asset

- **WHEN** a cook finishes successfully
- **THEN** the build-finished event SHALL be raised for the asset UUID and the asset SHALL become LOADED

#### Scenario: Failure leaves the asset FAILED

- **WHEN** a cook finishes with a failure
- **THEN** the event SHALL carry the failure and the asset SHALL become FAILED

### Requirement: Cook supports in-process and out-of-process runners

The system SHALL support cooking in-process (the same process as the editor) or out-of-process (a separate process), selectable by configuration. The loading layer SHALL depend only on the cook-completion event and MUST NOT depend on the cook mode.

#### Scenario: In-process cook completes in-process

- **WHEN** the in-process runner is configured and a cook is scheduled
- **THEN** the build SHALL run on a cook pool (not the loader pool) and raise the completion event in the same process

#### Scenario: Out-of-process cook raises the same event

- **WHEN** the out-of-process runner is configured and a cook is scheduled
- **THEN** the build SHALL run in a separate process and, on completion, the same in-process completion event SHALL be raised

### Requirement: Concurrency safety for load and cook

Loading and cooking SHALL not deadlock or lose state: an in-process cook MUST NOT occupy a loader worker while waiting, `product.index` writes SHALL be serialized per bundle, and a LOADING asset's wait handle SHALL be established before its cook is scheduled. On-demand coalescing SHALL be **atomic**: concurrent loads of the same UUID SHALL reuse the existing wait handle and MUST NOT overwrite it, exactly one cook SHALL be scheduled per in-flight UUID, and a scheduled cook SHALL NOT be dropped while a waiter depends on it. Reading the wait handle SHALL be synchronized (no unsynchronized read). File-product loads SHALL also coalesce: concurrent calls for the same UUID SHALL perform at most one header read/schedule and return the same `LOADING` or `LOADED` asset; recursive dependency loads SHALL NOT deadlock.

#### Scenario: No loader-pool starvation

- **WHEN** an on-demand cook is scheduled from a load running on the asset pool
- **THEN** the cook SHALL run without occupying a loader worker, so the pool cannot deadlock

#### Scenario: Concurrent index updates do not lose entries

- **WHEN** multiple cooks complete for the same bundle
- **THEN** each `product.index` update SHALL be serialized so no entry is lost

#### Scenario: Wait handle ready before cook

- **WHEN** a load returns a LOADING asset
- **THEN** its wait handle SHALL already be established so a blocking wait unblocks on LOADED or FAILED

#### Scenario: Concurrent loads coalesce without orphaning a waiter

- **WHEN** multiple threads load the same unbuilt asset concurrently
- **THEN** exactly one cook SHALL be scheduled, every caller SHALL share the same wait handle, and every blocking wait SHALL complete when the single cook finishes (no overwritten/orphaned wait handle)

#### Scenario: Late caller after a completed load does not re-cook

- **WHEN** a caller requests an asset whose load has already completed (LOADED or FAILED)
- **THEN** it SHALL NOT start a redundant second cook; a LOADED asset is returned as-is and a FAILED asset may be re-attempted

#### Scenario: File product concurrent load

- **WHEN** multiple threads load the same product UUID that already has a cooked product file
- **THEN** only one thread SHALL read the header/schedule the load, every caller SHALL receive a non-null asset, and all blocking waits SHALL complete

#### Scenario: File load already in flight

- **WHEN** a caller observes a file-backed asset with status `LOADING`
- **THEN** it SHALL return that asset without re-opening or re-reading the product

#### Scenario: Recursive file dependency load

- **WHEN** a file-product load recursively loads its dependencies
- **THEN** the load orchestration lock SHALL be recursive and SHALL NOT deadlock

### Requirement: Generated child requests have individual terminal results

Framework SHALL route a generated child cook request to its source owner and coalesce concurrent requests by owner and target. Every child waiter SHALL complete according to the presence and validity of its own requested member. Failed cooks and removed children SHALL produce explicit failure rather than indefinite Loading or inherited owner success.

#### Scenario: Concurrent children of one source
- **WHEN** two generated children require the same owner cook and target
- **THEN** one cook serves both requests and each child receives its own terminal result

#### Scenario: Child omitted by a successful cook
- **WHEN** the owner succeeds but the requested child no longer exists
- **THEN** that child completes with failure

### Requirement: Per-directory source manifest

The system SHALL persist source-asset identity in a per-directory manifest named `assets.jsonl` in every directory that contains registered source assets.

The manifest SHALL be UTF-8 with LF line endings and use **JSON Lines**: one JSON object per line, `{"file":"<filename>","id":"<uuid>"}`, optionally carrying a `"cook"` block (the asset cook config; its semantics are defined by the `asset-cook-config` capability). Lines MUST be sorted by `file` so output is deterministic and multi-branch merges stay line-based. The manifest SHALL be committed to version control in each writable source mount.

#### Scenario: Manifest created on first registration

- **WHEN** a source asset in a directory with no manifest is registered
- **THEN** the system SHALL create `assets.jsonl` in that directory containing a line with the new asset's filename and UUID

#### Scenario: Deterministic sorted output

- **WHEN** a directory manifest is written after adding or moving entries
- **THEN** its entries SHALL be ordered by filename and the file SHALL end with a newline

### Requirement: Stable one-time UUID assignment

Registering a source asset SHALL resolve its UUID from the directory manifest if an entry exists, otherwise SHALL generate a new UUID and persist it. After migration, the UUID of a source asset MUST NOT be recomputed from its path.

#### Scenario: Repeated registration is stable

- **WHEN** the same source path is registered more than once
- **THEN** the system SHALL return the same UUID each time

#### Scenario: Manifest entry is authoritative

- **WHEN** an asset already has a manifest entry and is registered again without a manifest change
- **THEN** the system SHALL return the manifest UUID and MUST NOT derive a new UUID from the path

### Requirement: Migration preserves existing references

Before identity assignment stops deriving from paths, a one-time migration SHALL scan the source mounts (filtered by builder-known extensions, including world files) and seed each manifest entry with its legacy path-derived UUID, computed from the asset's mount/legacy bundle role to match the old `(bundle, path)` scheme, so that references serialized by earlier builds continue to resolve. `assets.db`, when present, MAY be used as a cross-check but MUST NOT be required.

#### Scenario: Existing reference still resolves after migration

- **WHEN** a world or asset payload holds a UUID produced by the legacy path-derived scheme
- **THEN** after migration the resolver SHALL map that UUID to the same source asset

#### Scenario: Migration does not require assets.db

- **WHEN** migration runs on a fresh checkout with no `assets.db`
- **THEN** it SHALL still seed manifests from the scanned source tree and preserve existing references

### Requirement: Generated sources obtain identity through the resolver

Source documents whose identity was previously computed from their path (world documents) SHALL be registered as source assets and obtain their UUID through the same resolver, and migration SHALL cover them.

#### Scenario: World keeps its identity

- **WHEN** a world document resolves its identity after migration
- **THEN** its persist ID SHALL come from the resolver and match the pre-migration path-derived value

### Requirement: Source identity resolution

For identity and authoring (registration, build, mutation), the system SHALL resolve a source asset by path through the directory manifest (with a parsed-manifest cache) and by UUID through the identity map. Source identity resolution MUST NOT depend on `assets.db` being present. This is not an asset-data load path; asset data is always loaded from cooked products.

#### Scenario: Resolve by path

- **WHEN** `FindAsset(path)` is called for a migrated asset
- **THEN** the system SHALL return the source info whose UUID matches the directory manifest

#### Scenario: Resolve by UUID without assets.db

- **WHEN** `assets.db` is absent but directory manifests exist
- **THEN** resolving a known UUID SHALL still yield the correct source asset after the cache is rebuilt

### Requirement: Runtime lookup is independent of assets.db

The shipped runtime MUST NOT read `assets.db` or source `assets.jsonl` manifests. Runtime path-to-UUID lookup SHALL use a generated product index emitted into the product bundle; each index entry SHALL be a JSON Lines record `{"path","id"}` mapping a logical path to a UUID, and product payloads SHALL remain UUID-keyed.

#### Scenario: Runtime resolves a path without assets.db

- **WHEN** runtime calls `LoadAssetFromPath` in a build that ships no `assets.db`
- **THEN** the system SHALL resolve the UUID through the generated product index and load the product payload

#### Scenario: Runtime bundle contains no assets.db

- **WHEN** the runtime bundle is assembled
- **THEN** it SHALL contain the generated product index and MUST NOT contain `assets.db` or `assets.jsonl`

#### Scenario: Lookup spans multiple product bundles

- **WHEN** a runtime path→uuid lookup is made and products are split across bundles (e.g. textures in a platform bundle)
- **THEN** the resolver SHALL consult the merged indexes of all added product bundles

### Requirement: Mount-based logical namespace

Source roots SHALL be ordered mounts in the engine `MultiFileSystem` (workspace writable first, engine read-only next, with custom/pak/DLC adding more). Assets SHALL be keyed by a single logical path with no bundle field, and precedence SHALL be the mount order.

#### Scenario: Override by mount order

- **WHEN** the same logical path exists in two mounts
- **THEN** the earlier mount SHALL win

#### Scenario: Manifest from the owning mount

- **WHEN** a logical path is resolved
- **THEN** its UUID SHALL come from the `assets.jsonl` of the mount that owns the file

### Requirement: Single asset type identifier

The asset type SHALL be identified by one `AssetTypeId` shared by the type/handler registry and the product header. A source asset MUST NOT persist a separate type/category field.

#### Scenario: Source type is derived

- **WHEN** the system needs the type of a source asset
- **THEN** it SHALL derive it from the asset's extension through the builder registry, and SHALL NOT read a stored category field

#### Scenario: Product type comes from the header

- **WHEN** a product is loaded
- **THEN** the handler SHALL be selected by the `AssetTypeId` recorded in the product header

#### Scenario: No persisted category

- **WHEN** a manifest or dev-cache entry is written for a source asset
- **THEN** it SHALL contain no type/category field

### Requirement: Type identifier consistency

The `AssetTypeId` SHALL be the same identifier string across `AssetTraits<T>::ASSET_TYPE`, the builder registry's `QueryType(ext)`, the `AssetManager` handler-registry key, the product header `type`, and the editor property metadata (`SET_ASSET_TYPE`).

#### Scenario: Builder type resolves to a handler

- **WHEN** a builder reports a type for an extension
- **THEN** that type SHALL resolve to a registered handler and SHALL equal the type written into the product header

#### Scenario: Unknown builder type is rejected

- **WHEN** a builder reports a type for which no handler is registered
- **THEN** the registration or build SHALL be rejected rather than producing a product with an unloadable type

#### Scenario: Property type matches the asset type

- **WHEN** a property declares an asset type
- **THEN** that declared type SHALL equal the `AssetTypeId` of the assets it accepts, so type-based selection and validation agree with the handler and header type

### Requirement: Asset dependency provider

An `IAssetDependencyProvider` SHALL expose forward dependencies, reverse dependents, and full-graph iteration for assets.

#### Scenario: Forward dependencies

- **WHEN** the dependencies of an asset are queried
- **THEN** the provider SHALL return the direct dependencies recorded for that asset

#### Scenario: Reverse dependents

- **WHEN** the dependents of an asset are queried
- **THEN** the provider SHALL return the assets that reference it, computed by inverting the forward graph

### Requirement: Dependency data provenance

At runtime the forward graph SHALL be assembled from product headers; in the editor/build it SHALL be assembled from the source dependencies and persisted only as a derived cache. Dependencies SHALL NOT be stored in `assets.jsonl` or `product.index`.

#### Scenario: Runtime graph from product headers

- **WHEN** the runtime needs an asset's dependencies
- **THEN** it SHALL read them from the product header

#### Scenario: Editor graph not in the manifest or index

- **WHEN** the dependency graph is persisted in the editor
- **THEN** it SHALL live in the dev cache and MUST NOT appear in `assets.jsonl` or `product.index`

### Requirement: Dependency impact reporting

The provider SHALL allow tooling to query an asset's dependents before a destructive operation.

#### Scenario: Removing a referenced asset reports dependents

- **WHEN** an asset that other assets depend on is about to be deleted
- **THEN** the provider SHALL report its dependents so the operation can warn or be blocked

### Requirement: Generated catalog records preserve child identity and ownership

The existing source manifest and catalog SHALL record generated children by stable source-local key, UUID and type, together with their source owner and optional default-binding metadata. Recook and source moves SHALL preserve matching child identities; copying a source SHALL allocate independent child identities. Removed children SHALL no longer resolve as current catalog entries. Ambiguous or conflicting identity assignments SHALL fail explicitly.

#### Scenario: Move and copy a generated source
- **WHEN** a source with generated children is moved and subsequently copied
- **THEN** the move preserves its child UUIDs and the copy owns distinct child UUIDs

#### Scenario: Child dependency routes through its owner
- **WHEN** a builder registers a dependency on a generated child
- **THEN** the dependency retains the child UUID while recook routing resolves the actual owner source

### Requirement: Move and rename preserve identity

Moving or renaming a source asset SHALL keep its UUID unchanged by relocating the corresponding manifest entry to the destination directory, and MUST NOT modify any asset references.

#### Scenario: Rename in the same directory

- **WHEN** a source asset is renamed in place
- **THEN** its manifest entry SHALL be rewritten with the new filename and the same UUID

#### Scenario: Move to another directory

- **WHEN** a source asset is moved to a different directory within the writable mount
- **THEN** the old directory manifest entry SHALL be removed, the new directory manifest SHALL record the same UUID, and existing references SHALL continue to resolve

### Requirement: Delete removes identity

Deleting a source asset SHALL remove its manifest entry and its identity-map record; a manifest with no remaining entries SHALL be removed. Products generated from the deleted source are not removed by this operation and are reclaimed by a later build.

#### Scenario: Delete cleans identity

- **WHEN** a source asset is deleted
- **THEN** its file, manifest entry, and identity-map record SHALL all be removed

### Requirement: Duplicate assigns a new identity

Duplicating a source asset SHALL create the new file with a newly generated UUID and leave the original asset and its references unchanged. Two different source paths MUST NOT share a UUID in the manifests.

#### Scenario: Duplicate does not alias identity

- **WHEN** a source asset is duplicated
- **THEN** the duplicate SHALL have a distinct UUID and the original's UUID SHALL be unchanged

### Requirement: Content 模块与 adaptor / builder 分层

Content SHALL retain its existing adaptor, offline builder and runtime targets. Content.Adaptor SHALL provide Clip schema/conversion and the Clip source implementation using Animation and Framework interfaces. Content.Metadata SHALL own ContentReflection and persistent Clip handlers once per process definition. Content.Builder.Module SHALL depend on metadata and register Clip builders through RegisterMeta; it SHALL NOT depend on the runtime service. Content.Module SHALL depend on metadata and publish/revoke IAnimationClipSource in Init/Shutdown without registering reflection again. Graph schema/handlers SHALL belong to Animation.GraphAssets and Graph builders SHALL belong to Animation.Builder under engine/animation. Animation core SHALL remain independent of rendering and Framework implementation.

#### Scenario: Offline cook without runtime service
- **WHEN** Content.Metadata and Content.Builder.Module load without Content.Module
- **THEN** Clip schema, handler and builder are available without starting a runtime Clip provider

#### Scenario: Runtime session restart
- **WHEN** Content.Module shuts down and initializes again
- **THEN** its service is revoked and republished while the same metadata definition remains registered

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

#### Scenario: Graph builder 接管
- **WHEN** Animation.Builder 随工具模块加载
- **THEN** .graph SHALL 由其唯一处理，卸载或缺失时 SHALL 明确不可用而非退回旧空 Graph

### Requirement: Registered derived-data builders

The framework SHALL allow derived-data builders to be registered by a stable string id, and each
builder SHALL declare a version so that changing its build algorithm invalidates previously cached
results.

#### Scenario: Builder resolved by id

- **WHEN** a builder is registered under an id
- **THEN** the derived data cache SHALL resolve that builder by id for subsequent fetches

### Requirement: Content-addressed caching

The derived data cache SHALL store a builder's output under a key derived from the source bytes,
the builder id, the builder version, the settings string, and the platform. A fetch SHALL return the
stored output when the key is unchanged and SHALL run the builder and store the output on a miss;
changing any key input SHALL produce a miss.

#### Scenario: Cache hit avoids rebuild

- **WHEN** the same source, builder, version, settings, and platform are fetched twice
- **THEN** the builder SHALL run only once and both fetches SHALL return identical output

#### Scenario: Settings and platform are part of the key

- **WHEN** the same source is fetched with a different settings string or a different platform
- **THEN** the cache SHALL run the builder and store a separate entry

#### Scenario: Builder version invalidates

- **WHEN** the registered builder's version changes for otherwise identical inputs
- **THEN** the cache SHALL miss and run the builder again

### Requirement: Failure is reported without caching

The derived data cache SHALL report failure when no builder is registered for the requested id or
when the builder fails to build, and SHALL NOT write a cache entry in that case.

#### Scenario: Unknown builder

- **WHEN** a fetch is requested for an unregistered builder id
- **THEN** the cache SHALL report failure without writing an entry

### Requirement: Flat products have a checked versioned identity envelope

Each cooked asset SHALL use one UUID-addressed product file at `<uuid[0:2]>/<uuid>.bin`. The file SHALL begin with a 40-byte prefix containing magic `0x44525053`, version `1`, the UUID, exact body byte length and FNV-1a64 body checksum. The loader and inspection SHALL reject unsupported versions, wrong identity, truncation, trailing bytes and checksum mismatches before accepting decoded content. There SHALL be no group manifest or legacy/headerless product fallback.

#### Scenario: Corrupt or incompatible product
- **WHEN** a product has a wrong magic/version/UUID, invalid body length or checksum mismatch
- **THEN** loading and inspection fail explicitly without accepting a partial asset

#### Scenario: Development output reset
- **WHEN** the storage change is adopted before the first formal release
- **THEN** old cook output is cleared and rebuilt rather than migrated or read through another format

### Requirement: Checked product saves report the actual publication result

SaveAsset and SaveProducts SHALL return explicit ProductStatus. A batch SHALL encode/validate and stage all products with required write, flush and readback checks before replacing destinations. Each destination replacement SHALL be atomic. A failure SHALL propagate through the builder and package flow; no failed batch SHALL be reported as a successful cook. A partial replacement or index failure SHALL invalidate the affected batch, and cleanup failures SHALL remain explicit errors. No guarantee of restoring a previous complete multi-product batch is made.

#### Scenario: Encoding or staging fails
- **WHEN** an input fails encoding, a staged write/flush fails, or readback does not match
- **THEN** the batch reports failure and does not replace any destination

#### Scenario: Failure after one destination was replaced
- **WHEN** a later atomic replacement or product index update fails
- **THEN** the affected batch is invalidated and cannot be accepted as successful package output
- **AND** the save does not claim rollback to a previous complete group

### Requirement: Loaded asset instances retain active data

A successful save SHALL allow subsequent loads to create a new immutable loaded instance with its own instance ID/revision. Existing referenced assets SHALL retain their loaded data and bound bulk bytes while in use; bulk reads SHALL NOT reopen a UUID path and silently select a newer file. Renderer/GPU borrowers SHALL keep their exact data through existing retirement protocols. ProductsOnly loads and InspectProduct SHALL not require source data or initiate cook. Inspection SHALL expose checked formatVersion/checksum without a runtime group.

#### Scenario: Reload with retained bulk readers
- **WHEN** a product file is replaced while a consumer retains a loaded asset
- **THEN** the old asset's data and bulk reads remain bound to its selected content
- **AND** a later load returns a distinct instance rather than mutating the retained object

#### Scenario: Products-only inspection
- **WHEN** a checked product is inspected with no source catalog
- **THEN** type, dependencies, format version, checksum and payload are read from the product without loading dependencies or mutating the asset cache

### Requirement: Host World cooking consumes existing component metadata owners

When the Jolt builder is enabled, AssetTool SHALL load the existing `JoltPhysicsModule` through ModuleManager before cooking World documents containing PhysicsBody or Physics subsystem configuration. That module SHALL remain the unique metadata registration owner. The integration SHALL NOT introduce duplicate reflection, a new metadata target or module-loading protocol, or a builder target dependency on runtime/adaptor implementation. Physics schema and config validation SHALL apply to each target platform cooked by the host tool.

#### Scenario: Physics World cooked for mobile targets
- **WHEN** the host tool cooks a complete authored PhysicsBody and Physics subsystem config for iOS or Android
- **THEN** the existing metadata owner supplies their schemas and the tool publishes a checked flat binary World product

#### Scenario: Invalid physics World rejected
- **WHEN** a PhysicsBody omits a required persisted field or its Physics config requests an unsupported deterministic backend
- **THEN** cooking fails explicitly and publishes no World product
