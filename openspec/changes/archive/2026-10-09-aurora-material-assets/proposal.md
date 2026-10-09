## Why

`aurora-adaptor` 里现有的 `MaterialAssetData` 是旧模型的产物（`type` 只区分 value/texture、值固定 `float[4]`、单个 `shader` Uuid），和当前 `Material`（typed 属性 + `MaterialTechnique`）已经脱节；同时缺少 `MaterialInstance` 的资产（父材质 + 覆盖）。此外 material 引用 shader 的正确身份是 `ShaderRef`（路径 + entry，`aurora/shader/ShaderRef.h` 明确声明给 material/pass 用），而 `MaterialTechnique.shader` 目前却是具体对象 `CounterPtr<Shader>`。

## What Changes

- **`MaterialTechnique.shader` 由 `CounterPtr<Shader>` 改为 `ShaderRef`**（引擎里材质引用 shader 的正式类型；解析成具体 `Shader` 由渲染层的 `ShaderResolver` 负责）。
- **重写 `MaterialAssetData`**：带 `CURRENT_VERSION`；`techniques`（`tag` + `ShaderRef` + `MaterialRenderState` 逐字段）与 `properties`（`name` + `MaterialPropertyType` + 原始值字节 + 纹理 `Uuid`）。
- **新增 `MaterialInstanceAssetData`**：带 `CURRENT_VERSION`；父材质 `Uuid` + 值覆盖 + 纹理覆盖。
- 更新 `AssetTraits<Material>`；新增 `AssetTraits<MaterialInstance>`（`ASSET_TYPE = "AuroraMaterialInstance"`）。
- 新增运行时构建：`CreateMaterialFromAsset(Device *, const Asset<Material> &)` 与 `CreateMaterialInstanceFromAsset(Device *, const Asset<MaterialInstance> &)`（纹理经 `AssetManager` 解析；technique 只填 `ShaderRef`）。
- `AuroraReflection` 注册 `MaterialAssetData` / `MaterialInstanceAssetData` 与 `RegisterAssetHandler<MaterialInstance>()`。
- 新增往返测试。
- **BREAKING**：`MaterialTechnique.shader` 类型变更；`MaterialAssetData` 布局变更。

## Capabilities

### New Capabilities
<!-- none -->

### Modified Capabilities
- `aurora-material`：`MaterialTechnique` 以 `ShaderRef`（路径 + entry）引用 shader，而非具体 `CounterPtr<Shader>`。
- `aurora-adaptor`：运行时资产层新增 `Material`/`MaterialInstance` 的 typed 资产数据与 `CreateMaterialFromAsset` / `CreateMaterialInstanceFromAsset` 构建路径。

## Impact

- 代码：`engine/aurora/core/include/aurora/resource/Material.h`、`engine/aurora/core/test/MaterialTest.cpp`；`engine/aurora/adaptor/{include/aurora/adaptor/assets,src,test}`、`engine/aurora/adaptor/src/AuroraReflection.cpp`。
- 无 RHI/后端改动；无离线 cook 改动（运行时加载）。
