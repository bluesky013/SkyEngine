## ADDED Requirements

### Requirement: mesh 源导入

cook SHALL 通过 assimp 从内存导入 `.gltf`/`.glb`/`.fbx`/`.obj`，处理链沿用 legacy 标志集（Triangulate/GenSmoothNormals/FlipUVs/CalcTangentSpace/LimitBoneWeights/OptimizeGraph/OptimizeMeshes/PopulateArmatureData)。每个 aiMesh 生成一个 primitive（烘焙节点变换），材质按名去重为槽位表，骨骼收集为全局骨表（boneNames + inverseBindMatrices）与逐顶点 joints/weights。

#### Scenario: OBJ quad 导入

- **WHEN** 内存导入含 pos/uv/normal 的 OBJ quad
- **THEN** 产出 1 个 primitive，索引为三角形列表，法线/uv/tangent（CalcTangentSpace）齐全，材质名进槽位表

#### Scenario: 无效源拒绝

- **WHEN** 导入无法解析的字节
- **THEN** 返回失败且不产出 primitive

### Requirement: 交错顶点流组装

cook SHALL 把 primitive 合并为单条交错顶点流：属性按固定顺序（POSITION/NORMAL/TANGENT/UV1/COLOR/JOINTS/WEIGHTS）依配置与源可用性取舍，格式 POSITION/NORMAL=F_RGB32、TANGENT/COLOR/WEIGHTS=F_RGBA32、UV1=F_RG32、JOINTS=U_RGBA8；法线/切线经 inverse-transpose 变换；index 类型按顶点数自动选 U16/U32；bounds 从烘焙后位置计算。

#### Scenario: 属性表与 stride

- **WHEN** 组装含 pos/normal/tangent/uv 的源
- **THEN** 属性表紧凑排列（offset 累加等于 stride=48),vertexData/indexData 尺寸与计数一致

#### Scenario: tangent 可关

- **WHEN** 配置 `tangents=false`
- **THEN** 属性表不含 TANGENT

### Requirement: meshopt 优化与 meshlet

配置 `optimize=true` 时 SHALL 执行 meshopt vertex cache → overdraw → vertex fetch（顶点重映射）,submesh 索引范围保持不变。配置 `meshlets=true` 时 SHALL 构建 meshlet（默认 64 顶点/124 三角形）并计算包围球/锥剔除 bounds。

#### Scenario: 优化保持拓扑计数

- **WHEN** 对 cooked mesh 执行优化
- **THEN** index 计数与 submesh 范围不变，vertex/index 数据尺寸一致

#### Scenario: meshlet 段

- **WHEN** 开启 meshlets cook 一个 quad
- **THEN** 产出 1 个 meshlet，vertices/triangles/bounds 三段与 meshlet 计数一致

### Requirement: MeshAssetData v2 与 Skin 资产

`MeshAssetData` SHALL 携带顶点属性表、vertexStride、vertexCount/indexCount、indexType、可选 skin uuid 与可选 meshlet 段；`CURRENT_VERSION` 为 2，版本不匹配 SHALL 拒绝并清空。蒙皮 SHALL 产出独立 `AuroraSkin` 资产（inverseBindMatrices + boneNames + boneMapping),mesh 资产 SHALL 存其 uuid 并 `AddDependencies`。

#### Scenario: v2 往返

- **WHEN** writer 填充的 v2 资产经 Save/Load
- **THEN** 全部字段（属性表/计数/meshlet 段/bounds）一致

#### Scenario: v1 拒绝

- **WHEN** Load 版本为 1 的产物
- **THEN** 数据清空并记录错误日志

#### Scenario: 蒙皮依赖

- **WHEN** cook 蒙皮网格且 `skinning=true`
- **THEN** 产出独立 Skin 资产，mesh 资产 skin uuid 有效且依赖表含该 uuid

### Requirement: builder 注册与 presets

`AuroraMeshBuilder` SHALL 注册扩展名 `{.gltf,.glb,.fbx,.obj}`，`QueryType` 返回 "AuroraMesh";`mesh_build_presets.json` SHALL 提供 `defaultBundle` + 每 bundle 的 `tangents`/`optimize`/`meshlets`/`skinning` 开关，未知 bundle SHALL 回退默认并告警。

#### Scenario: presets 解析

- **WHEN** 加载含两个 bundle 的 presets json
- **THEN** 指定 bundle 命中，未知 key 回退 defaultBundle
