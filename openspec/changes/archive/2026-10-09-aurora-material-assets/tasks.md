## 1. Core：technique shader → ShaderRef

- [x] 1.1 `MaterialTechnique.shader` 由 `CounterPtr<Shader>` 改为 `ShaderRef`；include `aurora/shader/ShaderRef.h`
- [x] 1.2 更新 `MaterialTest.Technique`（shader 用 `ShaderRef`）

## 2. Material 资产

- [x] 2.1 重写 `MaterialAsset.h`：`MaterialTechniqueData` / `MaterialPropertyData` / `MaterialAssetData`（版本 + 逐字段 `MaterialRenderState` 序列化）+ `AssetTraits<Material>`
- [x] 2.2 新增 `MaterialInstanceAsset.h`：`MaterialInstanceAssetData`（版本 + 父材质 + 值/纹理覆盖）+ `AssetTraits<MaterialInstance>`

## 3. 构建与注册

- [x] 3.1 新增 `MaterialAsset.cpp`：`CreateMaterialFromAsset` / `CreateMaterialInstanceFromAsset`
- [x] 3.2 `AuroraReflection.cpp` 注册 `MaterialAssetData` / `MaterialInstanceAssetData` + `RegisterAssetHandler<MaterialInstance>()`

## 4. 测试与验证

- [x] 4.1 新增 `MaterialAssetTest` / `MaterialInstanceAssetTest`（往返 + 版本守卫）
- [x] 4.2 重跑 cmake（GLOB）→ 构建 → 跑 `Aurora.CoreTest` / `Aurora.AdaptorTest`
