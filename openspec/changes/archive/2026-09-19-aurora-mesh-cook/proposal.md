## Why

aurora 侧目前没有任何 mesh 导入/cook 能力（assimp/meshoptimizer 只在 legacy `render/builder` 里）。同时 aurora 的 `MeshAssetData` 只存裸 vertex/index 字节 + submesh + 材质槽，没有顶点属性表、index 类型、meshlet、蒙皮引用，cook 产物无法被运行时重建为 `Mesh`/`RenderGeometry`。

按已定的 cook 模块结构（`AuroraCook.Static` 单库，image/mesh/skin 平级子目录）补充 mesh cook。

## What Changes

- **MeshAssetData v2**（`aurora/adaptor/assets/MeshAsset.h`)：新增顶点属性表（semantic+semanticIndex+format+offset)、vertexStride、vertexCount/indexCount、indexType、可选 meshlet 段（meshlets/meshletVertices/meshletTriangles/bounds 四块裸数据）、可选 skin 资产 uuid;`CURRENT_VERSION` 1→2。
- **新增 Skin 独立资产**(`aurora/adaptor/assets/SkinAsset.h`):`SkinAssetData`（inverseBindMatrices + boneNames + boneMapping),`AssetTraits<Skin>` = "AuroraSkin"/BIN；注册进 `AuroraReflection`。
- **cook/mesh 模块**（对齐 image cook 模式）:
  - `MeshSource`:assimp 从内存导入 .gltf/.glb/.fbx/.obj，提取顶点属性/索引/材质分组/骨骼。
  - 处理链（`MeshProcess` 基类 + Payload 阶段）:`MeshAssembler`（交错顶点流）、`MeshOptimizer`(meshopt vertex cache/overdraw/vertex fetch)、`MeshletBuilder`(meshopt meshlet + bounds，可关）。
  - `MeshAssetWriter`:CookedMesh→`MeshAssetData`，蒙皮→`SkinAssetData`。
  - `AuroraMeshBuilder`:`AssetBuilder` 实现，扩展名 `{.gltf,.glb,.fbx,.obj}`，`QueryType`→"AuroraMesh"；蒙皮网格额外产出 Skin 资产并 `AddDependencies`。
  - `MeshBuildPresets`:`mesh_build_presets.json`(tangents/optimize/meshlets 开关 + bundle 解析，对齐 image presets 结构）。
- **CMake**:cook 层 glob 增加 `mesh/src` + `mesh/include`，链接 `3rdParty::assimp` + `3rdParty::meshoptimizer`（两包 MacOS-x86 已构建）。
- **测试**:`cook/test/MeshCookTest.cpp`(OBJ 内存解析、组装 stride/offset、优化保持计数、meshlet 段、writer 往返 + 版本守卫、JSON presets)。

## Capabilities

### New Capabilities

- `aurora-mesh-cook`: mesh 源导入、处理链、资产产物、builder 注册。
- 蒙皮资产并入 `aurora-mesh-cook`（独立 Skin asset 类型）。

### Modified Capabilities

- `aurora-resource`（若已有 spec 覆盖 MeshAssetData 则扩展；否则新能力涵盖）。

## Impact

- **接口/格式破坏**:`MeshAssetData` 版本 1→2，旧 v1 资产不兼容（dev 分支无存量资产，strict version guard 拒绝并清空）。
- **修改文件**:`engine/aurora/adaptor`(MeshAsset.h、新增 SkinAsset.h、AuroraReflection.cpp)、`engine/aurora/cook/`（新增 mesh/、test、CMakeLists)、`engine/configs/mesh_build_presets.json`（新增）。
- **三方依赖**：新增 assimp + meshoptimizer 链接（均已构建）。
- **不涉及**：blendshape（后续 change);meshlet 的运行时消费（RDG/mesh shader 管线未就绪，cook 仅产出数据）。
