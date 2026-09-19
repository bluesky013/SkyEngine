## MODIFIED Requirements

### Requirement: 运行时资产层

SHALL 为 `aurora::Mesh` / `Material` / `Texture` / `LodGroup` 提供 `AssetTraits<T>`（`DataType` + `ASSET_TYPE` + `SERIALIZE_TYPE`）、asset data 结构及 `Json`/`Bin` 存取，并在 `AuroraReflection` 中 `AssetManager::RegisterAssetHandler<T>()`。本 change SHALL 只做运行时加载，SHALL NOT 接离线 `AssetBuilderManager` 构建产物。

#### Scenario: 注册资产处理器

- **WHEN** `AuroraReflection` 完成
- **THEN** `AssetManager` 已注册 `Mesh`/`Material`/`Texture`/`LodGroup` 的 handler

#### Scenario: 资产数据往返

- **WHEN** 序列化再反序列化对应 asset data
- **THEN** 字段一致

### Requirement: 资产驱动组件

组件类型 SHALL 位于命名空间 `sky::aurora` 且 SHALL NOT 再加 `Aurora` 前缀（命名空间已限定）。SHALL 提供 `StaticMeshComponent`（`StaticMeshComponentData{ Uuid mesh; Uuid material; castShadow; receiveShadow; }`）、`LodGroupComponent`（`LodGroupComponentData{ Uuid lodGroup; castShadow; receiveShadow; }`）、`CameraComponent`、以及按光源类型拆分的 `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`，均派生 `ComponentAdaptor<Data>` 并注册到 `ComponentFactory`（分组 `"Aurora"`）。资产引用成员 SHALL 用 `Uuid` 且 SHALL 以 `SET_ASSET_TYPE(AssetTraits<T>::ASSET_TYPE)` 标注。

#### Scenario: 组件注册到组件层

- **WHEN** 查询 `ComponentFactory::GetTypes()` 的 `"Aurora"` 分组
- **THEN** 含 `StaticMeshComponent` / `LodGroupComponent` / `CameraComponent` / `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`

#### Scenario: 资产成员带类型元数据

- **WHEN** 取 `StaticMeshComponent` 的网格成员节点或 `LodGroupComponent` 的 LodGroup 成员节点
- **THEN** 其 `CommonPropertyKey::ASSET_TYPE` 为对应资产类型

#### Scenario: 光照按类型拆分

- **WHEN** 取 `DirectLightComponent` / `PointLightComponent` / `SpotLightComponent`
- **THEN** 各自 `Data` 只含该光源类型的字段（方向光无 range/cone；点光无 cone；聚光含 inner/outer cone）
