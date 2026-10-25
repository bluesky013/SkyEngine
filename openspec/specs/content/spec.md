# content Specification

## Purpose
TBD - created by archiving change aurora-cook-builders. Update Purpose after archive.
## Requirements
### Requirement: Content 模块与 adaptor / builder 分层

Content SHALL retain its existing adaptor, offline builder and runtime targets. Content.Metadata SHALL own ContentReflection and persistent handlers for existing Content asset schemas once per process definition. Content.Builder.Module SHALL depend on metadata and register its builders through RegisterMeta; it SHALL NOT require the runtime service for reflection. Content.Module and Content.Views SHALL depend on metadata and SHALL NOT invoke ContentReflection. New asset schemas, providers or ownership transfers SHALL be delivered with their feature implementation.

#### Scenario: Offline cook without runtime service
- **WHEN** Content.Metadata and Content.Builder.Module load without Content.Module
- **THEN** the existing asset schemas, handlers and builders are available without starting Content runtime services

#### Scenario: Runtime session restart
- **WHEN** Content.Module shuts down and initializes again
- **THEN** the same shared metadata definition remains registered without re-running ContentReflection

### Requirement: 动画资产（AnimationClip / Animation graph）

`engine/content/adaptor` SHALL 提供：

- `AnimationClipAssetData`（`version = CURRENT_VERSION`；`name`、`frameRate`、`skeleton` Uuid、`nodeChannels`）与 `AssetTraits<AnimationClip>`（`DataType = AnimationClipAssetData`，`BIN`）；`Save`/`Load` 逐通道（`position`/`scale`/`rotation` 的 `times`/`keys`）序列化；`Load` 在版本不匹配时拒绝。
- `AnimationAssetData`（`version = CURRENT_VERSION`）与 `AssetTraits<Animation>`（`BIN`）。
- `CreateAnimationClipFromAsset(asset)` → `AnimationClip`（含通道与帧率/帧数）。

`ContentReflection` SHALL 注册 `AnimationClipAssetData` / `AnimationAssetData` 与 `RegisterAssetHandler<AnimationClip>()` / `<Animation>()`（幂等）。

#### Scenario: 动画资产往返

- **WHEN** 序列化再反序列化含通道的 `AnimationClipAssetData`
- **THEN** `version` / `name` / `frameRate` / `skeleton` / 各通道 `times` 与 `keys` 一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `AnimationClipAssetData`
- **THEN** 拒绝加载（清空并记录）

### Requirement: Animation builder（.clip / .graph）

`AnimationBuilder : AssetBuilder`（`engine/content/builder`）SHALL 声明扩展名 `.clip` / `.graph`；`QueryType` SHALL 对 `.clip` 返回 `AssetTraits<AnimationClip>::ASSET_TYPE`、对 `.graph` 返回 `AssetTraits<Animation>::ASSET_TYPE`。`Request` SHALL 对 `.clip` 以二进制读入 `AnimationClipAssetData`、压缩通道、按 `skeleton`(`Uuid`) 写 product 依赖（是已注册源时兼登 source 依赖）并写回；对 `.graph` 以 JSON 读入 `AnimationAssetData` 并写回。资产引用统一用 `Uuid`。`Content.Builder.Module` SHALL 在 `Init` 调 `ContentReflection` 并注册 `AnimationBuilder`；`Content.Module`（运行时）SHALL 只调 `ContentReflection`。

#### Scenario: .clip 产出 AnimationClip

- **WHEN** 构建一个 `.clip` 源
- **THEN** 产出 `AnimationClipAssetData`；通道被压缩；`skeleton` 非空时登记 product 依赖（是已注册源时兼登 source 依赖）

#### Scenario: 注册 builder

- **WHEN** `Content.Builder.Module` 模块 `Init` 完成
- **THEN** `AssetBuilderManager` 对 `.clip` / `.graph` 返回非空 builder，且 `AssetManager` 已注册 `AnimationClip` / `Animation` 处理器
