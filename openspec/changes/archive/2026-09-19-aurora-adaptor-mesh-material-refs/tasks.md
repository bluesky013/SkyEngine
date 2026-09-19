## 1. Mesh 资产数据

- [x] 1.1 在 `engine/aurora/adaptor/include/aurora/adaptor/assets/MeshAsset.h` 的 `MeshAssetData` 增加 `static constexpr uint32_t CURRENT_VERSION = 1;` 与 `uint32_t version = CURRENT_VERSION;`
- [x] 1.2 在 `MeshAssetData` 增加 `std::vector<Uuid> materials;`（材质槽表），并添加 `const Uuid *GetMaterialUuid(uint32_t materialIndex) const` 解析访问器（空槽表返回 nullptr；越界回退槽 0）
- [x] 1.3 扩展 `Save`：先写 `version`，再写 `materials` 数量 + 每个 Uuid 字符串
- [x] 1.4 扩展 `Load`：读取 `version`，`version != CURRENT_VERSION` 时清空数据并记录错误返回；否则读取 `materials`
- [x] 1.5 确认 `MeshSubMeshData.materialIndex` 注释标明其为 `materials` 的索引，避免再次歧义

## 2. 组件材质语义

- [x] 2.1 在 `StaticMeshComponent.h` 为 `StaticMeshComponentData::material` 添加「材质槽 0 覆盖」语义注释（空 = 用资产 `materials[0]`；非空 = 仅覆盖槽 0）
- [x] 2.2 更新 `SetMaterialUuid` / `GetMaterialUuid` 注释，指向槽 0 覆盖契约（字段仍为 `Uuid`，保持反射成员与 `SET_ASSET_TYPE` 不变）
- [x] 2.3 如新增解析辅助（如按槽取材质），确保不引入设备依赖，保持在 adaptor 层

## 3. 测试

- [x] 3.1 在 `engine/aurora/adaptor/test/` 新增 mesh 资产测试文件（参照 `ImageAssetTest.cpp` 风格）
- [x] 3.2 覆盖 `materials` + `version` 的 Bin `Save`/`Load` 往返一致
- [x] 3.3 覆盖 `GetMaterialUuid` 的命中、空槽表、越界回退槽 0
- [x] 3.4 覆盖 `version != CURRENT_VERSION` 时加载被拒绝（数据为空）
- [x] 3.5 确认 `engine/aurora/adaptor/CMakeLists.txt` 的 `AuroraAdaptorTest` 通过 `GLOB_RECURSE TEST_FILES` 拾取新测试，无需改 CMake

## 4. 验证

- [x] 4.1 构建 `Aurora.Adaptor`（Windows）确认编译通过
- [x] 4.2 在 `SKY_BUILD_TEST` 下构建并运行 `AuroraAdaptorTest`，全部通过
- [x] 4.3 跑仓库 clang-format / clang-tidy 策略，确认新增代码符合规范
