## MODIFIED Requirements

### Requirement: 运行时资产层

SHALL 为 `aurora::Mesh` / `Material` / `MaterialInstance` / `Texture` / `LodGroup` 提供 `AssetTraits<T>`（`DataType` + `ASSET_TYPE` + `SERIALIZE_TYPE`）、asset data 结构及 `Json`/`Bin` 存取，并在 `AuroraReflection` 中 `AssetManager::RegisterAssetHandler<T>()`。本 change SHALL 只做运行时加载，SHALL NOT 接离线 `AssetBuilderManager` 构建产物。

#### Scenario: 注册资产处理器

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `Mesh`/`Material`/`MaterialInstance`/`Texture`/`LodGroup` 的 handler

#### Scenario: 资产数据往返

- **WHEN** 序列化再反序列化对应 asset data
- **THEN** 字段一致

## ADDED Requirements

### Requirement: Material 资产（typed 属性 + technique + 版本）

`MaterialAssetData`（`aurora/adaptor/assets/MaterialAsset.h`）SHALL 带 `version`（`CURRENT_VERSION`），含 `techniques` 与 `properties`：

- `MaterialTechniqueData` SHALL 为 `{ std::string tag; ShaderRef shader; MaterialRenderState renderState; }`；`ShaderRef` 以路径 + entry 存取；`MaterialRenderState`（`DepthStencil` / `RasterState` / `blendStates` / `alphaToCoverage`）SHALL **逐字段**序列化，SHALL NOT 对含 padding 的结构体做裸字节拷贝。
- `MaterialPropertyData` SHALL 为 `{ std::string name; uint32 type(=MaterialPropertyType); std::vector<uint8_t> value; Uuid texture; }`；uniform 值以原始字节存（大小由 `SizeOfPropertyType(type)` 派生），纹理以 `Uuid` 存；`bool` 作为 1 字节 value。
- `Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（清空并记录）。

#### Scenario: 往返

- **WHEN** 以 technique（含 renderState）+ typed 属性序列化再反序列化 `MaterialAssetData`
- **THEN** `version` / `techniques`（tag、shader 路径与 entry、renderState 各字段）/ `properties`（name、type、value、texture）一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `MaterialAssetData`
- **THEN** 拒绝加载（清空并记录）

### Requirement: MaterialInstance 资产（父材质 + 覆盖 + 版本）

`MaterialInstanceAssetData`（`aurora/adaptor/assets/MaterialInstanceAsset.h`）SHALL 带 `version`（`CURRENT_VERSION`），含父材质 `Uuid material`、值覆盖 `valueOverrides`（`{ std::string name; uint32 type; std::vector<uint8_t> value; }`）与纹理覆盖 `textureOverrides`（`{ std::string name; Uuid texture; }`）。`AssetTraits<MaterialInstance>` SHALL 以 `ASSET_TYPE = "AuroraMaterialInstance"`。`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载。

#### Scenario: 往返

- **WHEN** 序列化再反序列化含父材质与覆盖的 `MaterialInstanceAssetData`
- **THEN** `version` / `material` / `valueOverrides` / `textureOverrides` 一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `MaterialInstanceAssetData`
- **THEN** 拒绝加载（清空并记录）

### Requirement: CreateMaterialFromAsset / CreateMaterialInstanceFromAsset

SHALL 提供：

- `CounterPtr<Material> CreateMaterialFromAsset(Device *device, const Asset<Material> &asset)`
- `CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(Device *device, const Asset<MaterialInstance> &asset)`

`CreateMaterialFromAsset` SHALL 建 `Material`：按 `techniques` 填 `tag` + `ShaderRef` + `MaterialRenderState`（SHALL NOT 解析具体 shader），按 `properties` 写值 / 占纹理槽（纹理经 `AssetManager` 解析 `Uuid → Asset<Texture>` 后 `CreateTextureFromAsset(device, …)`）。`CreateMaterialInstanceFromAsset` SHALL 经 `AssetManager` 解析父材质资产并应用覆盖。设备为空或版本不匹配 SHALL 返回空 `CounterPtr` 且不崩溃。

#### Scenario: 构建材质

- **WHEN** 用合法 `MaterialAssetData` + 非空 device 调 `CreateMaterialFromAsset`
- **THEN** 返回的 `Material` 含对应 technique（`ShaderRef` 相等）与属性默认值

#### Scenario: 无 Device

- **WHEN** `device` 为空
- **THEN** 返回空 `CounterPtr`，不崩溃
