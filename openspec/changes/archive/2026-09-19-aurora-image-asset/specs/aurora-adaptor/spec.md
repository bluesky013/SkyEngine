## ADDED Requirements

### Requirement: 图像资产支持 2D / 2DArray / 3D 与版本

`ImageAssetData`（`aurora/adaptor/assets/ImageAsset.h`）SHALL 携带版本号 `version`（`CURRENT_VERSION`）与图像类型 `ImageAssetType`（`TEXTURE_2D` / `TEXTURE_2D_ARRAY` / `TEXTURE_3D` / `TEXTURE_CUBE`），并 SHALL 含 `format`(`PixelFormat`) / `width` / `height` / `depth` / `mipLevels` / `arrayLayers`，以及 `slices`（`ImageSliceHeader{offset,size,mipLevel, layer, depth}`）与 `rawData`（字节载荷）。`Save`/`Load` SHALL 支持上述布局；`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（保持空数据并记录），SHALL NOT 静默按新布局解析旧数据。

#### Scenario: 2D / 2DArray / 3D / Cube 往返

- **WHEN** 分别以 `TEXTURE_2D` / `TEXTURE_2D_ARRAY` / `TEXTURE_3D` / `TEXTURE_CUBE` 序列化再反序列化
- **THEN** `version` / `type` / `format` / `width` / `height` / `depth` / `mipLevels` / `arrayLayers` / `slices` / `rawData` 一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的数据
- **THEN** 拒绝加载（不产生错误尺寸/类型的资源），并记录错误

### Requirement: CreateTextureFromAsset 构建纹理

SHALL 提供 `CounterPtr<Texture> CreateTextureFromAsset(Device *device, const Asset<Texture> &asset)`，按 `ImageAssetType` 映射 `Image::Descriptor` 并创建对应 `Texture` 子类：`TEXTURE_2D` → `Texture2D`（`IMAGE_2D`，`arrayLayers=1`）、`TEXTURE_2D_ARRAY` → `Texture2DArray`（`IMAGE_2D`，`arrayLayers=N`）、`TEXTURE_3D` → `Texture3D`（`IMAGE_3D`，`extent.z=depth`）、`TEXTURE_CUBE` → `TextureCube`（`IMAGE_2D`，`arrayLayers=6`，`viewUsage=CUBE_MAP_COMPATIBLE`）。它 SHALL 遍历 `slices` 以 `ImageUploadRequest{mipLevel, layer, imageExtent}` 上传 `rawData` 的对应区段。

#### Scenario: 按类型创建并上传

- **WHEN** 用 `TEXTURE_2D_ARRAY`（`arrayLayers > 1`）的资产调用 `CreateTextureFromAsset`
- **THEN** 得到 `Texture2DArray`，且每个 slice 按 `mipLevel`/`layer` 上传

#### Scenario: Cubemap 创建

- **WHEN** 用 `TEXTURE_CUBE`（`arrayLayers=6`）的资产调用 `CreateTextureFromAsset`
- **THEN** 得到 `TextureCube`（`viewUsage` 含 `CUBE_MAP_COMPATIBLE`），6 个面按 layer 上传

#### Scenario: Cube layer 数非法

- **WHEN** `TEXTURE_CUBE` 的 `arrayLayers != 6`
- **THEN** 拒绝创建（返回空 `CounterPtr`）

#### Scenario: 无效资产

- **WHEN** 资产版本不匹配或 `device` 为空
- **THEN** 返回空 `CounterPtr`，不崩溃
