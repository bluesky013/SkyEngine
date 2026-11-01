# asset-cook Specification

## Purpose

Define offline asset conversion, builder settings, cook configuration and process communication.

## Scope notes

### asset-cook-ipc

The out-of-process asset cook path: the length-prefixed frame protocol, the
in-process/out-of-process `ICookRunner` seam, the `AssetTool` worker host, response
correlation, post-cook index visibility, and worker lifecycle.

### aurora-mesh-cook

aurora mesh cook 管线:assimp 源导入、交错顶点流组装、meshopt 优化/可选 meshlet、AuroraMesh(v6)+ 独立 AuroraSkin(v2) 资产产物、builder 注册与 presets 配置。

## Requirements

### Requirement: Builders self-describe effective settings

An asset builder SHALL be able to describe, for a given product bundle, the effective settings it
would apply, as ordered key/value pairs. The base implementation SHALL return no settings so existing
builders remain valid.

#### Scenario: Default is empty
- **WHEN** a builder does not override the settings description
- **THEN** describing any bundle SHALL return an empty list

#### Scenario: Builder reports its settings
- **WHEN** an image builder that encodes per bundle is asked to describe a bundle
- **THEN** it SHALL return the effective settings (e.g. encode, srgb, max size, mip generation) as
  key/value pairs

### Requirement: Settings queryable by asset extension

The builder manager SHALL expose a lookup that returns the settings description for an extension and
bundle, resolving to the builder registered for that extension.

#### Scenario: Lookup by extension
- **WHEN** the manager is queried with a registered extension and a bundle
- **THEN** it SHALL return that builder's settings description for the bundle

#### Scenario: Unknown extension
- **WHEN** the manager is queried with an extension that has no builder
- **THEN** it SHALL return an empty list

### Requirement: Source import flow

Importing a source asset SHALL copy it into the writable mount (workspace), assign it a stable identity (a manifest entry), and create its asset-level cook-config entry. Import MUST NOT modify any asset references, and MAY cook the current platform immediately (configurable, default on).

#### Scenario: Import assigns identity

- **WHEN** a source file is imported into the workspace
- **THEN** it SHALL receive a manifest identity and an asset-level cook-config entry, and no reference SHALL change

#### Scenario: Import cooks the current platform

- **WHEN** import is configured to cook immediately
- **THEN** the system SHALL schedule a cook for the current platform's target(s)

### Requirement: Project and asset cook configuration

A project-level cook config SHALL define platform-to-target presets and per-asset-kind settings (encoder, mips, compression). Each source asset SHALL be able to override these via the `cook` block of its line in the per-directory `assets.jsonl` manifest. The effective settings SHALL be the asset override applied over the project preset for the current platform. Both the project cook config and the asset manifest SHALL be human-editable text.

#### Scenario: Asset override wins

- **WHEN** an asset-level config sets a value that the project preset also sets
- **THEN** the asset-level value SHALL be used for that asset

#### Scenario: Project default applies

- **WHEN** an asset has no override for a setting
- **THEN** the project preset value for the current platform SHALL apply

### Requirement: Per-asset user cook config

An asset's `cook` block SHALL support a free-form `user` object for asset-specific builder parameters; the effective cook configuration SHALL expose it to the builder.

#### Scenario: User parameters reach the builder

- **WHEN** an asset's `cook.user` sets a builder-specific parameter
- **THEN** the effective cook configuration passed to the builder SHALL include it

### Requirement: Multi-platform target outputs

A source asset configured for several targets SHALL emit one product per target; all products SHALL share the source UUID, each SHALL live in its target bundle, and each bundle's product index SHALL map the logical path to that UUID.

#### Scenario: One texture, two platform products

- **WHEN** a texture is configured for both a BC target and an ASTC target
- **THEN** two products SHALL be emitted under the same UUID, one per target bundle

### Requirement: Cook data compression

Cook MAY compress product data. The product header SHALL record the compression codec, and the loader SHALL decompress via the Framework CompressionManager/ICompressor interface. Framework SHALL provide default LZ4 when SKY_BUILD_COMPRESSION is enabled, without a dynamically loaded compression module. Disabling the built-in SHALL preserve uncompressed loading and explicit unsupported-codec failures. Operations SHALL retain acquired provider handles until their calls finish. Hosts SHALL establish the shared manager before loading modules; acquired handles SHALL retain replacement-provider objects through use and destruction while defining DLL code remains process-resident.

#### Scenario: Compressed product round-trips

- **WHEN** a target is configured to compress and Framework built-in LZ4 is enabled
- **THEN** the loader SHALL decompress the product before deserialization without loading any codec module

#### Scenario: Uncompressed needs no codec

- **WHEN** a product is not compressed
- **THEN** loading SHALL NOT require a compression implementation or module

#### Scenario: Support disabled

- **WHEN** built-in compression is disabled and no explicit codec replacement is provided
- **THEN** requesting an LZ4 product operation SHALL fail explicitly without loading an optional module or changing the encoding

#### Scenario: Replacement removed during retained use

- **WHEN** an acquired provider remains referenced while its registry replacement is removed or disabled
- **THEN** the acquired provider object SHALL remain valid until that handle is released, with defining DLL code remaining process-resident, while subsequent acquisition observes the current selection

### Requirement: Cook worker protocol integrity

The out-of-process cook worker SHALL frame its protocol on stdout with a defined length-prefixed encoding and SHALL send logs to stderr so frames are never corrupted; it SHALL start with the same mount namespace and platform target as the editor.

#### Scenario: Logs do not corrupt the protocol

- **WHEN** the worker writes logs while serving requests
- **THEN** logs SHALL go to stderr and stdout SHALL carry only protocol frames

#### Scenario: Worker shares the mount namespace

- **WHEN** the worker resolves a source path
- **THEN** it SHALL use the same mounts and platform target as the editor

### Requirement: Length-prefixed frame protocol

The out-of-process cook worker SHALL exchange messages as length-prefixed frames over the child's standard output, where each frame is a little-endian unsigned 32-bit payload length followed by a UTF-8 JSON payload. The reader SHALL reassemble frames across partial reads and SHALL reject a declared length above a configured maximum as a desynchronized channel.

#### Scenario: Reassemble a split frame

- **WHEN** a frame's length header and payload arrive across multiple reads
- **THEN** the reader SHALL deliver exactly one complete message with the original payload bytes

#### Scenario: Oversized frame rejected

- **WHEN** a frame declares a payload length above the maximum
- **THEN** the channel SHALL fail with a desynchronization error instead of allocating unbounded memory

### Requirement: Log separation

Worker logs SHALL be written to standard error, and standard output SHALL carry only protocol frames so logs can never corrupt the protocol stream.

#### Scenario: Logs do not corrupt the protocol

- **WHEN** the worker writes log lines while serving requests
- **THEN** those lines SHALL be read from standard error and forwarded to the engine logger, and standard output SHALL contain only valid frames

#### Scenario: Malformed stdout fails the session

- **WHEN** a non-frame byte sequence appears on standard output
- **THEN** the reader SHALL treat the channel as failed and fail the in-flight requests rather than misinterpreting the stream

#### Scenario: Default logger cannot corrupt frames

- **WHEN** the worker runs engine logging during a cook, even though the engine logger's default stream is standard output
- **THEN** the worker SHALL direct the logger to standard error before engine initialization, and SHALL also preserve a private descriptor for frames and redirect process standard output to standard error, so standard output carries only frames even if a non-logger component writes to standard output directly

### Requirement: Protocol version handshake

On startup the worker SHALL perform a version handshake before accepting cook requests, and a protocol version mismatch SHALL fail the session.

#### Scenario: Handshake succeeds

- **WHEN** the worker starts and the parent sends a hello with a supported protocol version
- **THEN** the worker SHALL reply that it is ready together with its platform target

#### Scenario: Version mismatch rejected

- **WHEN** the parent's protocol version is not supported by the worker
- **THEN** the session SHALL fail and no cook request SHALL be sent

### Requirement: Mode-agnostic cook completion

The loading layer SHALL depend only on an `ICookRunner` abstraction and the build-finished event, so that an in-process cook and an out-of-process cook raise the same completion event and are indistinguishable to the loader. The cook mode SHALL be selectable by configuration, defaulting to in-process.

#### Scenario: Same event for both modes

- **WHEN** a cook requested through the out-of-process runner completes
- **THEN** it SHALL raise the same build-finished event, carrying uuid, target, return code, and error, as an in-process cook

#### Scenario: In-process default

- **WHEN** no cook mode is configured
- **THEN** cooks SHALL run in-process on the cook pool

#### Scenario: Out-of-process selected

- **WHEN** the configuration selects the out-of-process mode
- **THEN** cooks SHALL be dispatched to the worker process instead of running in the editor process

### Requirement: Request and response correlation

Each cook request SHALL carry a unique identifier and the asset uuid and target; the worker SHALL echo the identifier, uuid, target, return code, and any error in its response. A result SHALL be matched to its in-flight request by identifier, and the uuid and target SHALL be preserved for the completion event.

#### Scenario: Result matched to request

- **WHEN** the worker returns a result carrying the identifier of a queued request
- **THEN** the runner SHALL complete exactly that request with the returned return code and error

#### Scenario: Unknown response dropped

- **WHEN** a result arrives whose identifier does not match any in-flight request
- **THEN** the runner SHALL discard it and record a warning

### Requirement: Worker shares the editor context

The worker SHALL start with the same mount namespace and platform target as the editor and SHALL report its platform target in the handshake, so that path resolution and product placement match the editor.

#### Scenario: Same mounts and target

- **WHEN** the worker resolves a source path and writes a product
- **THEN** it SHALL use the same mounts and platform target as the editor

#### Scenario: Identity is stable

- **WHEN** the worker cooks an asset
- **THEN** the produced product SHALL keep the source uuid and the target bundle's product index SHALL map the logical path to that uuid

### Requirement: Worker capability parity

The worker SHALL provide the same asset-building capabilities as the editor by loading the same builder modules before serving requests; linking the framework alone SHALL NOT be assumed to provide builders. On a platform where the required builders are not built, a cook request SHALL fail observably rather than report success.

#### Scenario: Worker has builders

- **WHEN** the worker starts
- **THEN** it SHALL register the same builders as the editor (loaded from the builder modules) before accepting cook requests

#### Scenario: Missing builders fail the cook

- **WHEN** a cook is requested on a worker that has no builder for the asset
- **THEN** the request SHALL complete with a failure result and the pending load SHALL become failed

### Requirement: Post-cook index visibility

When an out-of-process cook completes successfully, the editor SHALL resolve the cooked asset by its logical path, which requires refreshing the editor's in-memory product mapping; the editor SHALL NOT rely on a mapping read only at bundle registration.

#### Scenario: Path resolves after out-of-process cook

- **WHEN** an out-of-process cook writes a product index entry for a path
- **THEN** a subsequent load of that path in the editor SHALL resolve to the cooked uuid

#### Scenario: Single writer

- **WHEN** the editor uses out-of-process cook
- **THEN** only the worker SHALL write the product index and manifests for that bundle, and the editor SHALL NOT run concurrent in-process cooks into the same bundle

### Requirement: Worker lifecycle and failure handling

The runner SHALL maintain a persistent worker per session, and on a per-request timeout, unexpected worker exit, or protocol failure it SHALL fail every in-flight request and permit a fresh worker to be started on the next request. A failed cook SHALL resolve the pending load to a failed state and SHALL NOT leave it stranded in the loading state.

#### Scenario: Timeout fails the request

- **WHEN** a cook request does not complete within its timeout
- **THEN** the request SHALL fail, the worker SHALL be terminated, and the pending load SHALL become failed

#### Scenario: Crash fails in-flight requests and restarts

- **WHEN** the worker exits unexpectedly while requests are in flight
- **THEN** all in-flight requests SHALL fail and a subsequent request SHALL start a fresh worker

#### Scenario: Clean shutdown on drain

- **WHEN** the runner is drained
- **THEN** the worker SHALL be asked to shut down and SHALL be terminated if it does not exit, leaving no orphan process

#### Scenario: Blocked reader threads unblock and join

- **WHEN** the worker is killed or drained while the parent's stdout/stderr reader threads are blocked on a read
- **THEN** those reads SHALL be unblocked, the threads SHALL join before the channel is destroyed, and shutdown SHALL complete without hanging or a use-after-free

### Requirement: Manifest per-asset cook override schema

The source manifest (`assets.jsonl`) entry SHALL support an optional `cook` object with a `targets` array
(listing the product bundles to cook for the asset) and a `settings` object keyed by **target/bundle**,
where each value is a **flat**, possibly **sparse** object of scalar settings (string/number/bool). A key
absent from `settings[target]` SHALL fall back to the global per-bundle preset. Non-scalar values and
`settings` keys that are not cooked targets SHALL be ignored.

#### Scenario: Parse targets and per-target settings

- **WHEN** an entry's `cook` block is `{"targets":["common","tex_mobile"],"settings":{"tex_mobile":{"maxSize":512,"generateMip":false}}}`
- **THEN** the target set is `["common","tex_mobile"]` and the `tex_mobile` override is `{maxSize:512, generateMip:false}`

#### Scenario: Sparse override falls back to the global preset

- **WHEN** a target override sets only `encode`
- **THEN** every other key (srgb/quality/block/maxSize/generateMip) is taken from the global preset for that bundle

#### Scenario: Ignore invalid entries

- **WHEN** a `settings` value is an object/array, or is keyed by a target absent from the cooked target set
- **THEN** the value is ignored and does not affect resolution

### Requirement: Effective cook settings resolution

The effective settings for an asset and target SHALL be the global builder preset for the target's bundle
overlaid key-by-key with `cook.settings[target]`. The effective target set SHALL remain: asset `targets` if
present, else project targets, else the active platform's preset bundles. The framework SHALL resolve this
and pass the flattened per-target override to the builder through the build request; builders SHALL NOT read
the manifest.

#### Scenario: Override wins over the global preset

- **WHEN** the `tex_mobile` global preset is ASTC 4x4 with mips and the asset override sets `generateMip=false`
- **THEN** the effective `tex_mobile` config has `generateMip=false` and the global ASTC 4x4 encode

#### Scenario: Unset key uses the global preset

- **WHEN** an asset has a `tex_pc` override that sets only `quality`
- **THEN** the effective `tex_pc` `encode`/`srgb`/`block`/`maxSize`/`generateMip` equal the global `tex_pc` preset values

#### Scenario: Asset targets override project targets

- **WHEN** the manifest lists `targets:["tex_mobile"]` and the project declares other targets
- **THEN** the asset is cooked only for `tex_mobile`

#### Scenario: Override travels with the build request

- **WHEN** the framework builds asset `X` for target `T`
- **THEN** `AssetBuildRequest.settings` carries `X`'s resolved `T` override, for both the in-process and the worker cook path

### Requirement: Manifest write API for cook overrides

`AssetDataBase` SHALL provide a setter that replaces the `cook` block of a writable asset's manifest entry
while preserving the entry's `file` and `id` (and any unrelated fields), and SHALL reject the write for a
read-only mount. Writing SHALL persist to the manifest and invalidate the manifest cache.

#### Scenario: Persist an override and reload

- **WHEN** a writable asset's `cook` block is set and the manifest cache is invalidated
- **THEN** reloading returns the written `cook` block with `file`/`id` unchanged

#### Scenario: Read-only mount rejects the write

- **WHEN** a `cook` write targets an asset on a read-only mount
- **THEN** the write is rejected and the manifest is unchanged

### Requirement: Builder-declared reflected settings type

An `AssetBuilder` SHALL declare a reflected cook-settings type via `GetSettingsType()` (a type registered
with the process-wide `SerializationContext`), SHALL materialize the effective settings (preset overlaid
with the sparse override) into a reflected `Any` via `MakeSettings(bundle, override)`, and SHALL compute the
sparse override that reproduces an edited settings object via `DiffSettings(bundle, edited)`. A builder
without a settings type SHALL expose none. The editor SHALL inspect and edit these settings **only** through
the generic reflected form (`ReflectedFormView` bound to `PropertyObject{obj, node}`), never through
hand-rolled per-field controls.

#### Scenario: Image builder declares its settings type

- **WHEN** the image builder's settings type is queried
- **THEN** it is a reflected type whose members are `encode` (enum NONE/BC7/ASTC), `srgb` (bool), `quality` (enum), `block` (int), `maxSize` (int), `generateMip` (bool)

#### Scenario: Effective settings reflect the override

- **WHEN** `MakeSettings(bundle, override)` is called with an override that changes `maxSize`
- **THEN** the returned reflected object has the override `maxSize` and the preset values for the other members

#### Scenario: Diff yields only changed keys

- **WHEN** `DiffSettings(bundle, edited)` is called with an object differing only in `maxSize`
- **THEN** the result contains only `maxSize`, so the persisted override is sparse

### Requirement: aurora cook 模块与 builder 注册

AuroraCook.Static SHALL retain the offline builder implementation and Aurora.Cook its module entry. Aurora.Cook SHALL depend on Aurora.Metadata and SHALL NOT invoke AuroraReflection. Its RegisterMeta SHALL register image cook settings and image/mesh/material/material-instance/LOD/skeleton builders once per process definition. Init/Shutdown SHALL NOT repeat persistent registration. Existing tool/editor build and module manifests SHALL load the declared metadata dependency before its consumers.

#### Scenario: Cook and runtime share Aurora metadata
- **WHEN** Aurora.Cook and Aurora.Render coexist or restart in the same process
- **THEN** shared asset handlers remain owned by Aurora.Metadata and builders remain registered once by Aurora.Cook

### Requirement: Image builder 契约与输出

`AuroraImageBuilder : AssetBuilder` SHALL 声明扩展名 `.png` / `.jpg` / `.jpeg` / `.hdr` / `.ktx` / `.image`，`QueryType(ext)` SHALL 返回 `AssetTraits<sky::aurora::Texture>::ASSET_TYPE`（`"AuroraTexture"`）。`Request` SHALL 产出 `sky::aurora::ImageAssetData`（`version = CURRENT_VERSION`），填充 `format` / `type` / `width`/`height`/`depth` / `mipLevels` / `arrayLayers` / 每个 mip+layer 的 `ImageSliceHeader` 与 `rawData`，并通过 `AssetManager::SaveAsset` 写入目标 bundle。SHALL 支持 2D、Cube（6 层）与 2D Array。

#### Scenario: png 产出 aurora 纹理

- **WHEN** 构建一个 `.png` 源
- **THEN** 生成 `sky::aurora::ImageAssetData`，`type = TEXTURE_2D`，`mipLevels >= 1`，`slices` 与 `rawData` 一致，且可被 `CreateTextureFromAsset` 消费

#### Scenario: cube 与 array 类型

- **WHEN** 源为 6 面 cube 或含多层 array
- **THEN** `type` 分别为 `TEXTURE_CUBE` / `TEXTURE_2D_ARRAY`，`arrayLayers` 与切片层数一致

### Requirement: 源解码与 KTX 读取

PNG/JPG/JPEG/HDR SHALL 通过 `stb_image` 解码为 RGBA8（HDR 为浮点）。`.ktx` SHALL 由内置 `KtxReader` 解析，支持 KTX1（`glInternalFormat`）与 KTX2（`vkFormat`）容器，映射为 `sky::aurora::PixelFormat`；KTX2 `supercompressionScheme != 0` SHALL 被拒绝并记录错误，SHALL NOT 静默解压或按错误布局解析。块压缩载荷（BC/ETC/ASTC）SHALL 原样透传，不做二次解码。

#### Scenario: 读取未压缩 KTX

- **WHEN** 读入 `supercompressionScheme == 0` 的 KTX1/KTX2
- **THEN** 正确解析维度、mip、layer 与格式，像素/块数据进入对应切片

#### Scenario: 拒绝超压缩 KTX2

- **WHEN** 读入 `supercompressionScheme != 0` 的 KTX2
- **THEN** 构建失败并记录错误，不产出资产

### Requirement: mip 生成与压缩管线

图像管线 SHALL 依序支持：最大尺寸缩放、可选 sRGB 线性化、mip 链生成（滤波核 SHALL 支持 `Box` / `Kaiser` / `Lanczos3`）、按目标格式压缩。分离式 polyphase 权重 SHALL 按每个输出采样中心的真实距离 `(tap - center)` 计算并归一化（SHALL NOT 使用会引入半像素偏移的共享窗口）。ASTC SHALL 通过 `astc-encoder` 静态库（`3rdParty::astc`）编码，BC SHALL 通过 `ispc_texcomp`（`3rdParty::ispc_texcomp`）编码 BC7。切片字节大小 SHALL 使用 `aurora/rhi` 的 `GetImageFormatInfo` / `GetImageRowPitch` / `GetImageSlicePitch` 的块对齐算法计算，SHALL NOT 复用 legacy `rhi` 的格式表。像素读取 SHALL 支持 `U8` / `HALF` / `Float` 三种源类型。

#### Scenario: 生成完整 mip 链

- **WHEN** `generateMip = true` 且源为 2D
- **THEN** `mipLevels == GetMipLevel(width, height)`，每级尺寸减半（下限 1）

#### Scenario: 选择滤波核

- **WHEN** mip 生成配置为 `Box` / `Kaiser` / `Lanczos3` 之一
- **THEN** 使用对应滤波核；纯色输入在归一化权重下保持原色

#### Scenario: 采样 tap 对齐

- **WHEN** 对 `[0,1,2,3]` 做精确 2x `Box` 下采样
- **THEN** 结果为 `[0.5, 2.5]`（相邻像素平均值），SHALL NOT 出现半像素偏移

#### Scenario: 16 位源滤波

- **WHEN** 源格式为 `R16`/`RGBA16`（`PixelType::HALF`）
- **THEN** 滤波按 half->float 读取像素，SHALL NOT 退化为全 0

#### Scenario: ASTC 与 BC 编码

- **WHEN** 目标格式为 ASTC（mobile）或 BC7（desktop）
- **THEN** 分别经 astc-encoder / ispc 产出块数据，`format` 为对应 `sky::aurora::PixelFormat`，切片大小按块对齐

#### Scenario: 关闭压缩回退未压缩

- **WHEN** 目标 bundle 未配置压缩格式
- **THEN** 产出未压缩 RGBA8，`format` 为对应非块格式

### Requirement: per-bundle 格式策略与产品 bundle

cook SHALL 提供 `configs/image_build_presets.json`，映射 bundle → 图像构建配置（`format` / `quality` / `block` / `maxSize` / `generateMip`），并含 `defaultBundle`。`AuroraImageBuilder::LoadConfig` SHALL 读取该配置；`Request` SHALL 以 `request.target`（当其为已知 bundle）否则 `defaultBundle` 解析输出 bundle。`AssetManager::GetBundle` SHALL 返回 key 精确匹配的 bundle；`target` 为空时 SHALL 返回第一个 bundle。SHALL NOT 返回第一个“不匹配”的 bundle。

#### Scenario: 按 bundle 选择格式

- **WHEN** `target = "tex_mobile"`
- **THEN** 使用该 bundle 的 ASTC 配置编码并写入 `tex_mobile` 产品目录

#### Scenario: 未指定 target 用默认 bundle

- **WHEN** `request.target` 为空
- **THEN** 使用 `defaultBundle` 的配置与产品 bundle

#### Scenario: 命名 bundle 精确匹配

- **WHEN** `GetBundle("tex_pc")` 且存在同名 bundle
- **THEN** 返回该 bundle；不存在时返回 `nullptr`（不返回其它 bundle）

### Requirement: Per-asset image cook override

`AuroraImageBuilder` SHALL apply the resolved per-asset override (from `AssetBuildRequest.settings`) over
the bundle config it resolves from `configs/image_build_presets.json`, for the keys `encode`, `srgb`,
`quality`, `block`, `maxSize`, and `generateMip`, before decoding/processing. `DescribeSettings(bundle,
override)` SHALL report the effective (global overlaid with override) values. An unrecognized override key
SHALL be ignored.

#### Scenario: Override resolution limit and mip generation

- **WHEN** the `tex_mobile` bundle is ASTC with `maxSize=1024`/`generateMip=true` and the asset override sets `maxSize=512`/`generateMip=false`
- **THEN** the produced texture is ASTC 4x4 (per the bundle), clamped to 512, with a single mip

#### Scenario: Override encode and block

- **WHEN** an asset's `tex_pc` override sets `encode=BC7` and `quality=SLOW`
- **THEN** the `tex_pc` product is encoded as BC7 at the SLOW quality

#### Scenario: Unset keys use the bundle preset

- **WHEN** an asset override sets only `srgb=false`
- **THEN** the effective encode/quality/block/maxSize/generateMip are the bundle preset values and `srgb=false` is applied

#### Scenario: ASTC block 6x6

- **WHEN** an asset's target override sets `encode=ASTC` and `block=6`
- **THEN** the product format is `ASTC_6x6` (6x6 is a supported block size alongside 4 and 8)

### Requirement: Material builder（.mat）

AuroraMaterialBuilder SHALL read the reflected typed .mat schema, validate MaterialAssetData and publish a versioned product with explicit technique stage/source/entry records and typed properties. New source values SHALL use canonical Material keys; legacy alias migration SHALL be checked against complete declaration metadata during preparation. Shader paths SHALL remain logical paths rather than UUID assets. Texture UUIDs SHALL register product and applicable source dependencies. A generic number/string JSON inference table SHALL NOT replace the typed source schema or shader-key validation.

#### Scenario: Typed material product
- **WHEN** a valid .mat with tagged stage programs and typed texture/value properties is cooked
- **THEN** the product preserves those values and records texture dependencies

#### Scenario: Invalid semantic value
- **WHEN** a property's stored value does not match its declared semantic type
- **THEN** cook fails before publishing a successful product

### Requirement: MaterialInstance builder（.mati）

AuroraMaterialInstanceBuilder SHALL read a typed MaterialInstanceSource, retain the parent UUID and persist value/texture/sampler overrides with semantic validation. Parent and texture UUIDs SHALL register product and applicable source dependencies. Complete key ownership and static-option restrictions SHALL be checked against the resolved parent schema during preparation; the builder SHALL NOT invent keys or shader layouts from JSON scalar shapes.

#### Scenario: Instance product
- **WHEN** a valid typed instance source is cooked
- **THEN** the product contains the parent and sparse overrides with their asset dependencies

#### Scenario: Wrong value type
- **WHEN** an override's semantic type disagrees with its stored value
- **THEN** cook returns failure without treating the mismatched payload as valid bytes

### Requirement: LodGroup builder（.lod）

`AuroraLodGroupBuilder : AssetBuilder` SHALL 声明扩展名 `.lod`，`QueryType` SHALL 返回 `AssetTraits<LodGroup>::ASSET_TYPE`。`Request` SHALL 解析 `.lod` JSON 产出 `LodGroupAssetData`（`version = CURRENT_VERSION`）：`levels` 每项 `{ screenSize, mesh(Uuid) }` → `LodGroupLevelData{ screenSize, mesh Uuid }`；mesh `Uuid` SHALL 写 product 依赖，是已注册源时兼登 source 依赖。

#### Scenario: .lod 产出 LOD 组

- **WHEN** 构建含多级 `{ screenSize, mesh }` 的 `.lod`
- **THEN** 产出 `LodGroupAssetData`；`levels[i].mesh` 为对应 mesh 资产 `Uuid` 且登记依赖

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

### Requirement: meshopt 优化与 meshlet

配置 `optimize=true` 时 SHALL 执行 meshopt vertex cache → overdraw → vertex fetch（顶点重映射）,submesh 索引范围保持不变。配置 `meshlets=true` 时 SHALL 构建 meshlet（默认 64 顶点/124 三角形）并计算包围球/锥剔除 bounds。

#### Scenario: 优化保持拓扑计数

- **WHEN** 对 cooked mesh 执行优化
- **THEN** index 计数与 submesh 范围不变，vertex/index 数据尺寸一致

#### Scenario: meshlet 段

- **WHEN** 开启 meshlets cook 一个 quad
- **THEN** 产出 1 个 meshlet，vertices/triangles/bounds 三段与 meshlet 计数一致

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

### Requirement: builder 注册与 presets

`AuroraMeshBuilder` SHALL 注册扩展名 `{.gltf,.glb,.fbx,.obj}`，`QueryType` 返回 "AuroraMesh";`mesh_build_presets.json` SHALL 提供 `defaultBundle` + 每 bundle 的 `tangents`/`optimize`/`meshlets`/`skinning` 开关，未知 bundle SHALL 回退默认并告警。

#### Scenario: presets 解析

- **WHEN** 加载含两个 bundle 的 presets json
- **THEN** 指定 bundle 命中，未知 key 回退 defaultBundle
