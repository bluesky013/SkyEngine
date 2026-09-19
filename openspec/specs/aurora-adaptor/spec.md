# aurora-adaptor Specification

## Purpose
TBD - created by archiving change aurora-adaptor. Update Purpose after archive.
## Requirements
### Requirement: adaptor 静态库 + runtime/editor 动态模块

`engine/aurora/adaptor` SHALL 为**纯静态库** `Aurora.Adaptor`（`AuroraModule` + `AuroraReflection` + assets + components）。launcher 动态模块 SHALL 位于 `engine/aurora/runtime`（target `AuroraRender` SHARED，仅 `AuroraRegistry.cpp` 注册 `AuroraModule`）；编辑器扩展模块 SHALL 位于 `engine/aurora/editor`（target `AuroraRender.Editor` SHARED，`AuroraEditorModule : AuroraModule`）。`Aurora.Adaptor` SHALL 链接 `Aurora` + `Framework`；`AuroraRender`/`AuroraRender.Editor` SHALL 链接 `Aurora.Adaptor`；`aurora/core` SHALL NOT 依赖 `framework`。

#### Scenario: 三目录产出

- **WHEN** 构建引擎
- **THEN** `engine/aurora/adaptor` 产出 `Aurora.Adaptor`（STATIC），`engine/aurora/runtime` 产出 `AuroraRender`（SHARED），`engine/aurora/editor` 产出 `AuroraRender.Editor`（SHARED）

#### Scenario: adaptor 不引入 framework 到 core

- **WHEN** 构建 `aurora/core`
- **THEN** 不引入 framework 依赖

### Requirement: AuroraReflection 入口

SHALL 提供 `void sky::aurora::AuroraReflection(sky::SerializationContext *context)`，风格与 `sky::CoreReflection` 一致，负责注册 aurora 类型、asset handler 与组件。它 SHALL 幂等。

#### Scenario: 注册并重复调用安全

- **WHEN** 调用 `AuroraReflection` 一次或多次
- **THEN** aurora 类型/组件可查到，且不触发重复注册断言

### Requirement: 运行时资产层

SHALL 为 `aurora::Mesh` / `Material` / `Texture` 提供 `AssetTraits<T>`（`DataType` + `ASSET_TYPE` + `SERIALIZE_TYPE`）、asset data 结构及 `Json`/`Bin` 存取，并在 `AuroraReflection` 中 `AssetManager::RegisterAssetHandler<T>()`。本 change SHALL 只做运行时加载，SHALL NOT 接离线 `AssetBuilderManager` 构建产物。

#### Scenario: 注册资产处理器

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `Mesh`/`Material`/`Texture` 的 handler

#### Scenario: 资产数据往返

- **WHEN** 序列化再反序列化对应 asset data
- **THEN** 字段一致

### Requirement: 资产驱动组件

组件类型 SHALL 位于命名空间 `sky::aurora` 且 SHALL NOT 再加 `Aurora` 前缀（命名空间已限定）。SHALL 提供 `StaticMeshComponent`（`StaticMeshComponentData{ Uuid mesh; Uuid material; castShadow; receiveShadow; }`）、`CameraComponent`、以及按光源类型拆分的 `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`，均派生 `ComponentAdaptor<Data>` 并注册到 `ComponentFactory`（分组 `"Aurora"`）。资产引用成员 SHALL 用 `Uuid` 且 SHALL 以 `SET_ASSET_TYPE(AssetTraits<T>::ASSET_TYPE)` 标注。

#### Scenario: 组件注册到组件层

- **WHEN** 查询 `ComponentFactory::GetTypes()` 的 `"Aurora"` 分组
- **THEN** 含 `StaticMeshComponent` / `CameraComponent` / `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`

#### Scenario: 资产成员带类型元数据

- **WHEN** 取 `StaticMeshComponent` 的网格成员节点
- **THEN** 其 `CommonPropertyKey::ASSET_TYPE` 为对应资产类型

#### Scenario: 光照按类型拆分

- **WHEN** 取 `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`
- **THEN** 各自 `Data` 只含该光源类型的字段（方向光无 range/cone；点光无 cone；聚光含 inner/outer cone）

### Requirement: 编辑器经扩展模块对接

编辑器 SHALL 通过**编辑器扩展模块**（`AuroraEditorModule : AuroraModule` + `REGISTER_MODULE`，由编辑器模块配置加载）对接 aurora，SHALL NOT 由应用入口显式调用 `AuroraReflection`。扩展模块 `Init` SHALL 走基类 `AuroraModule::Init`（内部调用 `AuroraReflection`）并注册编辑器侧扩展（actor/asset creator、preview）。资产成员 SHALL 走 `Uuid` + `ASSET_TYPE`，以便编辑器 `PropertyUuid` 拾取器无需改动即可工作。

#### Scenario: 编辑器经扩展模块加载 aurora

- **WHEN** 编辑器加载 aurora 编辑器扩展模块
- **THEN** 组件/资产元数据通过组件扩展路径可见，`"Aurora"` 分组组件出现且资产成员可编辑

#### Scenario: 运行时不依赖编辑器扩展

- **WHEN** launcher 运行 `AuroraModule::Init`
- **THEN** 反射/组件注册完成，无需编辑器扩展模块

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

