## 1. 资产格式

- [x] 1.1 `MeshAssetData` 升 v2：属性表/stride/count/indexType/skin uuid/meshlet 段 + 严格版本守卫
- [x] 1.2 新增 `SkinAsset.h`（SkinAssetData + AssetTraits<Skin> "AuroraSkin")
- [x] 1.3 `AuroraReflection` 注册 SkinAssetData + AssetHandler<Skin>

## 2. cook/mesh 实现

- [x] 2.1 `MeshSource`:assimp 内存导入（顶点属性/索引/材质分组/骨骼）
- [x] 2.2 `MeshProcess` 基类 + `MeshAssembler` 交错流组装
- [x] 2.3 `MeshOptimizer`(meshopt 三段 + 重映射）
- [x] 2.4 `MeshletBuilder`（可选）
- [x] 2.5 `MeshAssetWriter`(mesh + skin 产物）
- [x] 2.6 `MeshBuildConfig`/`MeshBuildPresets` + `configs/mesh_build_presets.json`
- [x] 2.7 `AuroraMeshBuilder` + 模块注册

## 3. 构建集成

- [x] 3.1 cook CMake glob + `sky_find_3rd(assimp/meshoptimizer)` + 链接

## 4. 测试

- [x] 4.1 `MeshCookTest.cpp`：源解析/组装/优化/meshlet/writer 往返/版本守卫/presets
- [x] 4.2 编译 + `AuroraCookTest` 全绿
