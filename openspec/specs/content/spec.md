# content Specification

## Purpose
TBD - created by archiving change aurora-cook-builders. Update Purpose after archive.
## Requirements
### Requirement: Content 模块与 adaptor / builder 分层

SHALL 新增独立模块 `engine/content`，承载**非渲染域**的运行时资产桥与其离线 builder（当前为 animation；后续 prefab 等同类资产也归此）。`engine/content` SHALL 拆为：

- `engine/content/adaptor`（`Content.Adaptor` STATIC）：动画资产数据 + `AssetTraits` + `FetchOrCreate`/`CreateXxxFromAsset` + `ContentReflection(SerializationContext*)`；`LINK_LIBS: Animation, Framework`。
- `engine/content/builder`（`Content.Builder.Static` STATIC + `Content.Builder.Module` SHARED）：`AssetBuilder` 实现；`LINK_LIBS: Content.Adaptor, Framework`；`Content.Builder.Module` 注册 builder。
- `engine/content/runtime`（`Content.Module` SHARED）：运行时模块，只注册资产 data/handler（`ContentReflection`），SHALL NOT 注册 builder。

`engine/content` SHALL 由 `engine/CMakeLists.txt` 以 `add_subdirectory(content)` 加入构建。`engine/animation`（`Animation`）SHALL 保持只依赖 `Core`，SHALL NOT 依赖 `Framework`；动画资产 SHALL NOT 放在 `render` / `aurora`。

#### Scenario: 分层与依赖

- **WHEN** 阅读 `engine/content/{adaptor,builder}` 的 CMake
- **THEN** `Content.Adaptor` 链 `Animation` + `Framework`；`Content.Builder.Static` 链 `Content.Adaptor` + `Framework`；`Animation` 仍只链 `Core`

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

