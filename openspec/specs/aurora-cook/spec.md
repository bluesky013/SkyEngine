# aurora-cook Specification

## Purpose
TBD - created by archiving change aurora-image-cook. Update Purpose after archive.
## Requirements
### Requirement: aurora cook 模块与 builder 注册

aurora cook SHALL 拆为 `AuroraCook.Static`（STATIC，全部解码/处理/编码/builder 逻辑）与 `Aurora.Cook`（SHARED，`IModule` + `REGISTER_MODULE`）。`Aurora.Cook::Init` SHALL 调用 `sky::aurora::AuroraReflection`（幂等）并向 `AssetBuilderManager` 注册 `AuroraImageBuilder`。cook 目录 SHALL 仅在 `SKY_BUILD_TOOL` 且在 Win32/macOS 下加入构建；`Aurora.Cook` SHALL 通过 `sky_add_dependency` 依赖 `Launcher`/`Editor`。编辑器在 cook 目标存在时 SHALL 加载 `Aurora.Cook`。

#### Scenario: 注册图像 builder

- **WHEN** `Aurora.Cook` 模块 `Init` 完成
- **THEN** `AssetBuilderManager::QueryBuilder(".png")` 返回非空，且 `AssetManager` 已注册 `AuroraTexture` 资产处理器

#### Scenario: 非工具构建不产出 cook

- **WHEN** 未开启 `SKY_BUILD_TOOL`
- **THEN** 不构建 `AuroraCook.Static` / `Aurora.Cook`

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

