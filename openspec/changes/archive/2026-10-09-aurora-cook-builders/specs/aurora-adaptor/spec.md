## MODIFIED Requirements

### Requirement: Mesh 资产材质槽引用与版本

`MeshAssetData`（`aurora/adaptor/assets/MeshAsset.h`）SHALL 携带 `version`（`CURRENT_VERSION`）与材质槽表 `std::vector<Uuid> materials`；`MeshSubMeshData.materialIndex` SHALL 作为该表的索引解析为材质资产 `Uuid`。`Save`/`Load` SHALL 序列化并反序列化 `version`、`materials`（`Uuid` 以字符串存取）与蒙皮/骨架引用 `Uuid skin` / `Uuid skeleton`，SHALL 提供 `const Uuid *GetMaterialUuid(uint32_t materialIndex)` 解析访问器。`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（保持空数据并记录），SHALL NOT 静默按新布局解析旧数据。

#### Scenario: 材质槽与版本往返

- **WHEN** 以非空 `materials` 与 `version = CURRENT_VERSION` 序列化再反序列化 `MeshAssetData`
- **THEN** `version` 与 `materials`（含顺序）一致

#### Scenario: 解析材质索引

- **WHEN** 某个 `MeshSubMeshData.materialIndex` 小于 `materials.size()`
- **THEN** `GetMaterialUuid(materialIndex)` 返回 `materials[materialIndex]`

#### Scenario: 越界索引回退

- **WHEN** 在非空槽表下某 `materialIndex >= materials.size()`
- **THEN** 解析回退到槽 0，并记录警告，不导致加载失败

#### Scenario: 空槽表

- **WHEN** `materials` 为空
- **THEN** 所有子网格解析为无绑定材质（按槽 0 处理），不崩溃

#### Scenario: skin / skeleton 引用往返

- **WHEN** 以非空 `skin` / `skeleton` 与 `version = CURRENT_VERSION` 序列化再反序列化 `MeshAssetData`
- **THEN** `skin` 与 `skeleton` 一致；未蒙皮时二者为空

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `MeshAssetData`
- **THEN** 拒绝加载（不产生错误索引/材质的资源），并记录错误

## ADDED Requirements

### Requirement: Skeleton 资产桥接

`engine/aurora/adaptor` SHALL 提供 `SkeletonAssetData`（`aurora/adaptor/assets/SkeletonAsset.h`，`version = CURRENT_VERSION`，包 `sky::SkeletonData`：`boneData`(`Name` + `parentIndex`) 与 `refPos`(`Transform`)，`Save`/`Load` 逐字段）与 `AssetTraits<sky::Skeleton>`（`DataType = SkeletonAssetData`，`ASSET_TYPE = "AuroraSkeleton"`，`BIN`），并提供 `FetchOrCreateSkeletonByAsset`（asset → `Skeleton::BuildSkeleton`）。`Aurora.Adaptor.Static` SHALL 链接 `Animation`（桥接层唯一的 Skeleton 依赖点）；`AuroraReflection` SHALL 注册 `SkeletonAssetData` 与 `RegisterAssetHandler<sky::Skeleton>()`。`aurora/core` SHALL NOT 定义 `Skeleton`/`Bone`。

#### Scenario: 注册骨架资产

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `sky::Skeleton` 的 handler，且 `AssetTraits<sky::Skeleton>::ASSET_TYPE == "AuroraSkeleton"`

#### Scenario: 骨架数据往返

- **WHEN** 序列化再反序列化 `SkeletonAssetData`
- **THEN** `version` / `boneData`（name、parentIndex）/ `refPos` 一致

#### Scenario: 构建运行时骨架

- **WHEN** 对一个含非空 `boneData` 的 `Asset<Skeleton>` 调 `FetchOrCreateSkeletonByAsset`
- **THEN** 得到非空 `SkeletonPtr`（`GetNumBones() == boneData.size()`）
