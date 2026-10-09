## Why

aurora cook 目前只有 `AuroraImageBuilder` / `AuroraMeshBuilder`，材质、材质实例、LOD 组没有离线 cook 产物；旧引擎 `engine/render/builder` 有 `MaterialBuilder`（`.mat`/`.mati`）与 `LodGroupBuilder`。补齐后这些资产才能由源文件烘焙成 `MaterialAssetData` / `MaterialInstanceAssetData` / `LodGroupAssetData`。

## What Changes

- 新增 `AuroraMaterialBuilder`（`.mat`，JSON）→ `MaterialAssetData`：内联 technique（`tag` + shader 路径/entry + `MaterialRenderState`）+ typed 属性。
- 新增 `AuroraMaterialInstanceBuilder`（`.mati`，JSON）→ `MaterialInstanceAssetData`：父材质路径 + 值/纹理覆盖。
- 新增 `AuroraLodGroupBuilder`（`.lod`，JSON）→ `LodGroupAssetData`：levels（`screenSize` + mesh 路径）。
- 新增 `AuroraSkeletonBuilder`（`.skeleton`，JSON）→ `SkeletonAssetData`（桥接 `animation::SkeletonData`）。
- **新增独立模块 `engine/content`**（非渲染域资产桥 + builder）：`Content.Adaptor`（动画资产 `AnimationClipAssetData` / `AnimationAssetData` + `AssetTraits` + `ContentReflection`，`LINK_LIBS: Animation, Framework`）+ `Content.Builder.Static` / `SkyContent.Builder` module（`AnimationBuilder`，`.clip`/`.graph`）。`Animation` core 保持只依赖 `Core`。
- adaptor 新增 **Skeleton 资产桥接**：`SkeletonAssetData` + `AssetTraits<sky::Skeleton>` + `FetchOrCreateSkeletonByAsset`；`Aurora.Adaptor.Static` 链接 `Animation`（桥接层唯一的 Skeleton 依赖点，`aurora/core` 仍不定义 Skeleton）。
- `MeshAssetData` 增 `Uuid skeleton`；mesh builder 在蒙皮源上除 `Skin` 外**再产出 `Skeleton` 资产**并按 Uuid 引用、登记依赖。
- `AuroraCookModule` 注册以上四个 builder。
- 属性 JSON 推断：`number` → `FLOAT`、`bool` → `BOOL`、`[2/3/4]` → `FLOAT2/3/4`、`string` → 纹理路径；可选 `{ "type": ..., "value": ... }` 显式指定（含 `INT`/`UINT`）。
- `renderState` 逐字段可选（缺省用默认值），支持常见子集（depth/stencil、raster、blend、alphaToCoverage）。
- **资产引用统一用 `Uuid`**（源里直接存 uuid 字符串）：texture / mesh / material / skeleton；**唯一例外是 Shader**（`ShaderRef` = 路径 + entry）。builder 用 `Uuid::CreateFromString` 解析，`AssetDataBase::Exists(uuid)` 判定是否为已注册源，据此写 product 依赖（始终）与 source 依赖（是源时）。
- skin：保持为 mesh cook 的产物，**不单列** builder。

## Capabilities

### New Capabilities
- `content`：独立的非渲染域资产桥模块（`Content.Adaptor` + `Content.Builder`），承载 animation 资产与 builder（后续 prefab 等同类资产归此）。

### Modified Capabilities
- `aurora-cook`：builder 注册新增 material / materialinstance / lodgroup / skeleton；新增四个 builder 的源格式与产出契约；mesh builder 蒙皮时额外产出 skeleton 资产。
- `aurora-adaptor`：新增 Skeleton 资产桥接（`SkeletonAssetData` + `AssetTraits<sky::Skeleton>` + handler）；`MeshAssetData` 新增 `skeleton` 引用。

## Impact

- 代码：`engine/aurora/cook/material/**`（新增）、`engine/aurora/cook/lodgroup/**`（新增）、`engine/aurora/cook/module/AuroraCookModule.cpp`、`engine/aurora/cook/CMakeLists.txt`。
- 依赖：`Aurora.Adaptor.Static` 的 `MaterialAssetData` / `MaterialInstanceAssetData` / `LodGroupAssetData`；JSON 走框架 `rapidjson`。
- 不改 RHI / 运行时；无新三方库。
