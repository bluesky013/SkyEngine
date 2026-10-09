## MODIFIED Requirements

### Requirement: aurora cook 模块与 builder 注册

aurora cook SHALL 拆为 `AuroraCook.Static`（STATIC，全部解码/处理/编码/builder 逻辑）与 `Aurora.Cook`（SHARED，`IModule` + `REGISTER_MODULE`）。`Aurora.Cook::Init` SHALL 调用 `sky::aurora::AuroraReflection`（幂等）并向 `AssetBuilderManager` 注册 `AuroraImageBuilder`、`AuroraMeshBuilder`、`AuroraMaterialBuilder`、`AuroraMaterialInstanceBuilder`、`AuroraLodGroupBuilder`、`AuroraSkeletonBuilder`。cook 目录 SHALL 仅在 `SKY_BUILD_TOOL` 且在 Win32/macOS 下加入构建；`Aurora.Cook` SHALL 通过 `sky_add_dependency` 依赖 `Launcher`/`Editor`。编辑器在 cook 目标存在时 SHALL 加载 `Aurora.Cook`。

#### Scenario: 注册 builder

- **WHEN** `Aurora.Cook` 模块 `Init` 完成
- **THEN** `AssetBuilderManager` 对 `.png` / `.gltf` / `.mat` / `.mati` / `.lod` / `.skeleton` 均返回非空 builder，且 `AssetManager` 已注册 `AuroraTexture` / `AuroraMaterial` / `AuroraMaterialInstance` / `AuroraLodGroup` / `AuroraSkeleton` 资产处理器

#### Scenario: 非工具构建不产出 cook

- **WHEN** 未开启 `SKY_BUILD_TOOL`
- **THEN** 不构建 `AuroraCook.Static` / `Aurora.Cook`

## ADDED Requirements

### Requirement: Material builder（.mat）

`AuroraMaterialBuilder : AssetBuilder` SHALL 声明扩展名 `.mat`，`QueryType` SHALL 返回 `AssetTraits<Material>::ASSET_TYPE`。`Request` SHALL 解析 `.mat` JSON 产出 `MaterialAssetData`（`version = CURRENT_VERSION`）：

- `techniques` SHALL 为内联数组，每项 `{ tag, shader(路径), entry }` → `MaterialTechniqueData{ tag, ShaderRef{relativePath, entry}, MaterialRenderState }`；`shader` SHALL 存为 `ShaderRef` 路径，SHALL NOT 解析或依赖 shader 资产。
- `renderState` 各字段 SHALL 可选（缺省用 `MaterialRenderState` 默认）。
- `properties` 值 SHALL 按类型推断（`number`→`FLOAT`、`bool`→`BOOL`、`[2/3/4]`→`FLOAT2/3/4`、`string`→纹理 `Uuid`），SHALL 支持 `{type,value}` 显式（含 `INT`/`UINT`/`MAT4`）。资产引用 SHALL 统一用 `Uuid`（仅 Shader 例外，用 `ShaderRef`）。纹理 `Uuid` SHALL 写 product 依赖；经 `AssetDataBase::Exists` 判定为已注册源时 SHALL 兼登 source 依赖。

#### Scenario: .mat 产出材质

- **WHEN** 构建含一个 technique 与若干属性的 `.mat`
- **THEN** 产出 `MaterialAssetData`；`techniques[0].shader.relativePath` 为源中的 shader 路径；属性 `type`/`value` 与 JSON 推断一致；纹理属性解析为纹理资产 `Uuid` 并登记依赖

#### Scenario: renderState 缺省

- **WHEN** technique 未提供 `renderState`
- **THEN** 使用 `MaterialRenderState` 默认值

### Requirement: MaterialInstance builder（.mati）

`AuroraMaterialInstanceBuilder : AssetBuilder` SHALL 声明扩展名 `.mati`，`QueryType` SHALL 返回 `AssetTraits<MaterialInstance>::ASSET_TYPE`。`Request` SHALL 解析 `.mati` JSON 产出 `MaterialInstanceAssetData`（`version = CURRENT_VERSION`）：`material` SHALL 为父材质 `Uuid`（写 product 依赖；是已注册源时兼登 source 依赖）；`properties` SHALL 解析为值/纹理覆盖（同 `.mat` 的类型推断，纹理覆盖为 `Uuid`）。

#### Scenario: .mati 产出材质实例

- **WHEN** 构建含 `material` 与覆盖属性的 `.mati`
- **THEN** 产出 `MaterialInstanceAssetData`；`material` 为父材质 `Uuid`（登记依赖）；`valueOverrides` / `textureOverrides` 与 JSON 一致

### Requirement: LodGroup builder（.lod）

`AuroraLodGroupBuilder : AssetBuilder` SHALL 声明扩展名 `.lod`，`QueryType` SHALL 返回 `AssetTraits<LodGroup>::ASSET_TYPE`。`Request` SHALL 解析 `.lod` JSON 产出 `LodGroupAssetData`（`version = CURRENT_VERSION`）：`levels` 每项 `{ screenSize, mesh(Uuid) }` → `LodGroupLevelData{ screenSize, mesh Uuid }`；mesh `Uuid` SHALL 写 product 依赖，是已注册源时兼登 source 依赖。

#### Scenario: .lod 产出 LOD 组

- **WHEN** 构建含多级 `{ screenSize, mesh }` 的 `.lod`
- **THEN** 产出 `LodGroupAssetData`；`levels[i].mesh` 为对应 mesh 资产 `Uuid` 且登记依赖

### Requirement: Skeleton builder（.skeleton）

`AuroraSkeletonBuilder : AssetBuilder` SHALL 声明扩展名 `.skeleton`，`QueryType` SHALL 返回 `AssetTraits<sky::Skeleton>::ASSET_TYPE`（`"AuroraSkeleton"`）。`Request` SHALL 解析 `.skeleton` JSON 产出 `SkeletonAssetData`（`version = CURRENT_VERSION`）：`bones` 数组每项 `{ name, parent(索引, -1 为根), translation[3], rotation[4] (x,y,z,w), scale[3] }` → `boneData`(`Name` + `parentIndex`) 与 `refPos`(`Transform`)。

#### Scenario: .skeleton 产出骨架

- **WHEN** 构建含多级 `bones` 的 `.skeleton`
- **THEN** 产出 `SkeletonAssetData`，`boneData[i].name`/`parentIndex` 与 `refPos[i]` 与 JSON 一致

### Requirement: Mesh builder 产出 skeleton

`AuroraMeshBuilder` SHALL 在源为蒙皮（`config->skinning && source.Skinned()`）时，除 `Skin` 资产外**再产出一个 `Skeleton` 资产**（骨骼层级 + 参考姿势，由 mesh 源的节点层级推导，索引与 `SkinAssetData::boneNames` 对齐），并把其 `Uuid` 写入 `MeshAssetData::skeleton` 且登记依赖。非蒙皮源 SHALL 令 `MeshAssetData::skeleton` 为空。

#### Scenario: 蒙皮网格引用 skeleton

- **WHEN** 构建蒙皮源
- **THEN** 生成 `AuroraSkin` 与 `AuroraSkeleton` 两个资产；`MeshAssetData.skin` 与 `MeshAssetData.skeleton` 均非空且登记为依赖

#### Scenario: 非蒙皮网格无 skeleton

- **WHEN** 构建非蒙皮源
- **THEN** `MeshAssetData.skeleton` 为空
