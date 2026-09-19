## ADDED Requirements

### Requirement: Mesh 资产材质槽引用与版本

`MeshAssetData`（`aurora/adaptor/assets/MeshAsset.h`）SHALL 携带 `version`（`CURRENT_VERSION`）与材质槽表 `std::vector<Uuid> materials`；`MeshSubMeshData.materialIndex` SHALL 作为该表的索引解析为材质资产 `Uuid`。`Save`/`Load` SHALL 序列化并反序列化 `version` 与 `materials`（`Uuid` 以字符串存取），SHALL 提供 `const Uuid *GetMaterialUuid(uint32_t materialIndex)` 解析访问器。`Load` SHALL 在 `version != CURRENT_VERSION` 时拒绝加载（保持空数据并记录），SHALL NOT 静默按新布局解析旧数据。

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

#### Scenario: 版本不匹配

- **WHEN** 加载 `version != CURRENT_VERSION` 的 `MeshAssetData`
- **THEN** 拒绝加载（不产生错误索引/材质的资源），并记录错误

### Requirement: StaticMeshComponent 材质槽 0 覆盖

`StaticMeshComponentData.material` SHALL 为可选的**材质槽 0 覆盖**：为空时 SHALL 使用 mesh 资产的 `materials[0]`；非空时 SHALL 仅覆盖槽 0，`materialIndex != 0` 的子网格 SHALL 仍使用资产 `materials[materialIndex]`。该资产引用成员 SHALL 保持 `Uuid` 并以 `SET_ASSET_TYPE(AssetTraits<Material>::ASSET_TYPE)` 标注。

#### Scenario: 空覆盖取资产槽 0

- **WHEN** `material` 为空且 mesh 资产含非空 `materials`
- **THEN** 槽 0 的子网格解析为资产 `materials[0]`

#### Scenario: 非空仅覆盖槽 0

- **WHEN** `material` 非空且 mesh 资产含多个槽
- **THEN** 槽 0 子网格解析为组件 `material`，其余子网格仍解析为资产 `materials[materialIndex]`

#### Scenario: 资产成员带类型元数据

- **WHEN** 取 `StaticMeshComponent` 的材质成员节点
- **THEN** 其 `CommonPropertyKey::ASSET_TYPE` 为 `AuroraMaterial`
