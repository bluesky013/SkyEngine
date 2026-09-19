## Context

镜像 image cook 的既有模式（详见 `aurora-image-cook` 归档）：`AssetBuilder` 契约（extensions/QueryType/LoadConfig/Request)、`MeshProcess` 阶段基类 + Payload、JSON presets + bundle Resolve、`WriteXxxAsset` 填充 `AssetTraits<T>::DataType` 后 `AssetManager::SaveAsset`、依赖经 `asset->AddDependencies(uuid)` 进产物头。

关键约束：

- `MeshAssetData` v1 无顶点布局信息，运行时无法重建 `RenderGeometry` 的 `VertexLayout`（`VertexBuffer` 携带 stride + `VertexSemanticMask`)。
- aurora 的 `Skin`(`aurora/resource/Skin.h`）自包含 inverseBindMatrices/boneMatrices/boneMapping，不含骨骼层级（Skeleton 属 animation 模块）。蒙皮资产只需持久化 bind 数据 + 骨骼名（运行时按名映射 animation skeleton)。
- assimp（v6.0.2,SHARED）与 meshoptimizer（v0.23）已是三方包且 MacOS-x86 已构建；legacy `render/builder` 的 assimp 处理链（Triangulate|GenSmoothNormals|FlipUVs|CalcTangentSpace|LimitBoneWeights|OptimizeGraph|OptimizeMeshes）与 meshlet 构建（`meshopt_buildMeshlets`+`meshopt_computeMeshletBounds`）是直接参照。
- `AssetBuildResult` 只有 retCode；依赖写入靠 `asset->AddDependencies` + `SaveAsset` 的产物头。

## Goals / Non-Goals

**Goals:**

- .gltf/.glb/.fbx/.obj → `AuroraMesh` 资产（交错顶点流 + 索引 + submesh + 材质槽 + bounds + 可选 meshlet)。
- 蒙皮网格 → 独立 `AuroraSkin` 资产，mesh 资产持其 uuid 并登记依赖。
- meshopt 优化默认开（可配置关）。

**Non-Goals:**

- blendshape cook;meshlet 的 GPU 消费管线；场景/节点层级与材质资产生成（legacy PrefabBuilder 职责，后续独立 change)。
- 运行时 `Mesh`/`RenderGeometry` 从 v2 资产的重建桥接（adaptor bridge 层后续 change;cook 只保证数据完备）。

## Decisions

### D1: MeshAssetData v2 字段

新增：`vertexStride`、`vertexCount`、`indexCount`、`indexType`(uint32)、`attributes[]`（{semantic:u8, semanticIndex:u8, format:u32, offset:u32}——不序列化 `VertexAttributeDesc` 的指针成员）、`skin`(Uuid，无效值=无蒙皮）、meshlet 段：`meshlets`/`meshletVertices`/`meshletTriangles`/`meshletBounds` 四块裸字节 + 各自计数（空=未构建）。v1 的 vertexData/indexData/subMeshes/materials/bounds 原样保留。严格版本守卫（!=2 → clear()）。

属性表用 `VertexSemantic` 枚举值而非字符串，运行时查表得 semantic 名；format 存 `Format`(rhi 顶点格式枚举）值。

### D2: 顶点流 = 单交错流

cook 产物只产一条交错 vertex stream（POSITION/NORMAL/TANGENT/UV1/COLOR/JOINTS/WEIGHTS 按属性掩码取舍）。引擎当前是 vertex pulling 模型（顶点输入为空），交错流配合 `VertexSemanticMask` 足够；多流拆分留给后续（如需要分离静态/蒙皮顶点）。

### D3: 蒙皮独立资产，按名桥接

`SkinAssetData`:inverseBindMatrices + boneNames + boneMapping。mesh 资产存 skin uuid 并 `AddDependencies(skin)`。骨骼层级不进 skin 资产（属 animation 资产）。运行时桥接：按 boneNames 对齐 animation skeleton 节点，填 `Skin::boneMatrices`。

skin 资产 uuid: cook 期 `Uuid::Create()` 生成（每次 cook 不同可接受，bundle 以 uuid 为文件名，旧产物自然被替换清理语义不覆盖——记录为已知取舍）。

### D4: assimp 内存导入 + 固定处理链

`Importer::ReadFileFromMemory(bytes, ext)`，处理链沿用 legacy 标志集（Triangulate/GenSmoothNormals/FlipUVs/CalcTangentSpace/LimitBoneWeights/OptimizeGraph/OptimizeMeshes/PopulateArmatureData)。多 mesh 节点合入单一 Mesh 资产：逐 aiMesh 生成 submesh（保留 node 变换烘焙到顶点），材质槽按 aiMaterial 索引去重。

### D5: meshopt 三段优化 + 可选 meshlet

`MeshOptimizer`:OptimizeVertexCache → OptimizeOverdraw → OptimizeVertexFetch（带顶点重映射回交错流）。`MeshletBuilder`:`meshopt_buildMeshlets`（默认 64/124)+ `meshopt_computeMeshletBounds`，产物为四块裸数据段。均可经 presets 关闭。

### D6: 配置/扩展名约定

`mesh_build_presets.json`:`{"defaultBundle": "...", "bundles": {"<key>": {"tangents": true, "optimize": true, "meshlets": false}}}`。builder 扩展名 `{.gltf,.glb,.fbx,.obj}`；`.mesh`（aurora 产物重 cook）预留不实现。
