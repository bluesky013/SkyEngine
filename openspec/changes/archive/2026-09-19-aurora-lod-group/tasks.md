## 1. Core LodGroup 资源

- [x] 1.1 新建 `engine/aurora/core/include/aurora/resource/LodGroup.h`：`sky::aurora::LodLevel { float screenSize = 1.f; CounterPtr<Mesh> mesh; }` 与 `LodGroup : RefObject`
- [x] 1.2 `LodGroup` 提供 `AddLevel`、`GetLevels`、`GetLevelCount`、`GetMesh(uint32_t)`（越界返回 nullptr）
- [x] 1.3 实现 `GetBoundingSphere()`：取 level 0 mesh 的 local bounds；无 level / mesh 为空返回空 `BoundingBoxSphere`
- [x] 1.4 实现 `SelectLod(float screenSize)`：在有序 levels 上按阈值选择层级索引
- [x] 1.5 实现 `SelectLod(const BoundingBoxSphere&, const Vector3& viewOrigin, const Matrix4& proj)`：复用 legacy `LodUtils` 的投影 screen size 公式后调用阈值选择（header-only，与 aurora core 现有资源一致）
- [x] 1.6 确认 `aurora/core` 不引入 framework / `Uuid`（仅 `CounterPtr<Mesh>`）

## 2. Adaptor LodGroup 资产

- [x] 2.1 新建 `engine/aurora/adaptor/include/aurora/adaptor/assets/LodGroupAsset.h`：`LodGroupLevelData { float screenSize; Uuid mesh; }` 与 `LodGroupAssetData { CURRENT_VERSION; version; levels; }`
- [x] 2.2 新建 `engine/aurora/adaptor/src/assets/LodGroupAsset.cpp` 实现 `Save`/`Load`（Bin，Uuid 字符串；version 不匹配则清空并 `LOG_E`）
- [x] 2.3 添加 `AssetTraits<sky::aurora::LodGroup>`（`DataType`/`ASSET_TYPE = "AuroraLodGroup"`/`BIN`）
- [x] 2.4 在 `AuroraReflection.cpp` 的 `ReflectAssetTypes` 注册 `LodGroupAssetData` 的 `BinLoad`/`BinSave`，并 `AssetManager::RegisterAssetHandler<sky::aurora::LodGroup>()`

## 3. LodGroupComponent

- [x] 3.1 新建 `engine/aurora/adaptor/include/aurora/adaptor/components/LodGroupComponent.h`：`LodGroupComponentData { Uuid lodGroup; bool castShadow; bool receiveShadow; }` 与 `LodGroupComponent : ComponentAdaptor<...>, IAssetReadyNotifier`（`SingleAssetHolder<sky::aurora::LodGroup>`）
- [x] 3.2 提供 `SetLodGroupUuid`/`GetLodGroupUuid`、`GetLodGroupAsset`/`IsLodGroupLoaded`，并在 `OnAssetLoaded` 中保持占位（资源构建为后续）
- [x] 3.3 在 `AuroraReflection.cpp` 实现 `LodGroupComponent::Reflect`：成员 `Uuid` + `SET_ASSET_TYPE(AssetTraits<aurora::LodGroup>::ASSET_TYPE)`，注册到 `ComponentFactory` 分组 `"Aurora"`
- [x] 3.4 确认 `StaticMeshComponent` 不变

## 4. 测试

- [x] 4.1 在 `engine/aurora/core/test/` 新增 `LodGroupTest.cpp`：阈值选择、越界 `GetMesh`、空组 `GetBoundingSphere`、投影重载与 `SelectLod(float)` 一致
- [x] 4.2 在 `engine/aurora/adaptor/test/` 新增 `LodGroupAssetTest.cpp`：`levels` + `version` 的 Bin `Save`/`Load` 往返一致
- [x] 4.3 覆盖 `version != CURRENT_VERSION` 时加载被拒绝（levels 为空）
- [x] 4.4 确认 `AuroraCoreTest` / `AuroraAdaptorTest` 通过 `GLOB_RECURSE TEST_FILES` 拾取新测试，无需改 CMake

## 5. 验证

- [x] 5.1 构建 `Aurora` + `Aurora.Adaptor`（Windows）确认编译通过
- [x] 5.2 在 `SKY_BUILD_TEST` 下构建并运行 `AuroraCoreTest` 与 `AuroraAdaptorTest`，全部通过
- [x] 5.3 按仓库 `.clang-format` / `.clang-tidy` 策略检查新增代码（clang-format 不可用时人工核对）
