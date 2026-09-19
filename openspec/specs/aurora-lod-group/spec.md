# aurora-lod-group Specification

## Purpose
TBD - created by archiving change aurora-lod-group. Update Purpose after archive.
## Requirements
### Requirement: aurora::LodGroup 核心资源

`aurora/resource/LodGroup.h` SHALL 定义 `sky::aurora::LodGroup : RefObject`，持有有序 `std::vector<LodLevel>`，其中 `LodLevel { float screenSize; CounterPtr<Mesh> mesh; }`。SHALL 提供 `AddLevel`、`GetLevels`、`GetLevelCount`、`GetMesh(uint32_t)`（越界返回 `nullptr`）、`GetBoundingSphere()`（取 level 0 mesh 的 local bounds；无 level 或 mesh 为空时返回空 sphere）与 `SelectLod`。`SelectLod(float screenSize)` SHALL 在有序 levels 上按阈值选择并返回层级索引；`SelectLod(const BoundingBoxSphere &bounds, const Vector3 &viewOrigin, const Matrix4 &proj)` SHALL 先由投影计算 screen size 再复用阈值选择。`LodGroup` SHALL NOT 依赖 framework 或 `Uuid`。

#### Scenario: 阈值选择层级

- **WHEN** levels 的 `screenSize` 阈值为有序值，且给定 screen size 命中某一层
- **THEN** `SelectLod` 返回对应层级索引

#### Scenario: 越界或空网格

- **WHEN** 传入的 level 索引越界，或 level 的 mesh 为空
- **THEN** `GetMesh` 返回 `nullptr`，不崩溃

#### Scenario: 空组包围球

- **WHEN** `LodGroup` 没有任何 level
- **THEN** `GetBoundingSphere` 返回空 sphere，不崩溃

#### Scenario: 投影重载选择

- **WHEN** 用 `BoundingBoxSphere` + `viewOrigin` + `proj` 调用 `SelectLod`
- **THEN** 结果与先用相同参数计算 screen size 再调 `SelectLod(float)` 一致

### Requirement: LodGroup 资产数据与版本

`LodGroupAssetData`（`aurora/adaptor/assets/LodGroupAsset.h`）SHALL 携带 `version`（`CURRENT_VERSION`）与有序 `std::vector<LodGroupLevelData>`，其中 `LodGroupLevelData { float screenSize; Uuid mesh; }`。SHALL 提供 `Save`/`Load`（Bin）；`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（清空数据并记录），SHALL NOT 静默按新布局解析旧数据。`AssetTraits<sky::aurora::LodGroup>` SHALL 为 `DataType = LodGroupAssetData`、`ASSET_TYPE = "AuroraLodGroup"`、`SERIALIZE_TYPE = BIN`。

#### Scenario: 层级与版本往返

- **WHEN** 以非空 `levels` 与 `version = CURRENT_VERSION` 序列化再反序列化 `LodGroupAssetData`
- **THEN** `version` 与 `levels`（含顺序、`screenSize`、`mesh`）一致

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `LodGroupAssetData`
- **THEN** 拒绝加载（保持空 levels），并记录错误

