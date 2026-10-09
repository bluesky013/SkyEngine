## 1. Material builder（`.mat` / `.mati`）

- [x] 1.1 `cook/material/` 共享源解析（technique / renderState / property 推断）
- [x] 1.2 `AuroraMaterialBuilder`（`.mat` → `MaterialAssetData`）
- [x] 1.3 `AuroraMaterialInstanceBuilder`（`.mati` → `MaterialInstanceAssetData`）

## 2. LodGroup builder（`.lod`）

- [x] 2.1 `AuroraLodGroupBuilder`（`.lod` → `LodGroupAssetData`）

## 3. 接线

- [x] 3.1 cook `CMakeLists.txt`：GLOB 加 `material/src/*` / `lodgroup/src/*` 与对应 include
- [x] 3.2 `AuroraCookModule::Init` 注册三个 builder

## 4. Skeleton

- [x] 4.1 adaptor 桥接：`SkeletonAsset.h`（`SkeletonAssetData` + `AssetTraits<sky::Skeleton>` + `FetchOrCreateSkeletonByAsset`）；`Aurora.Adaptor.Static` link `Animation`；`AuroraReflection` 注册 data + handler
- [x] 4.2 cook：`AuroraSkeletonBuilder`（`.skeleton` → `SkeletonAssetData`）
- [x] 4.3 `MeshAssetData` 增 `Uuid skeleton`（版本 3）；mesh cook 由节点层级推导骨骼（`CookBone` + `WriteSkeletonAsset`）并引用、登记依赖
- [x] 4.4 cook `CMakeLists` / `AuroraCookModule` 接线

## 5. Content 模块（animation 资产 + builder）

- [x] 5.1 `engine/content/adaptor`（`Content.Adaptor`，链 `Animation` + `Framework`）：`AnimationClipAssetData` / `AnimationAssetData` + `AssetTraits` + `CreateAnimationClipFromAsset` + `ContentReflection`
- [x] 5.2 `engine/content/builder`（`Content.Builder.Static` + `SkyContent.Builder` module）：`AnimationBuilder`（`.clip`/`.graph`）+ 注册
- [x] 5.3 `engine/content/CMakeLists.txt` + `engine/CMakeLists.txt` `add_subdirectory(content)`
- [x] 5.4 `Animation` core 保持只依赖 `Core`

## 6. 验证

- [x] 6.1 重跑 cmake（GLOB）→ 构建 `AuroraCook.Static` / `Aurora.Cook` / `Aurora.Adaptor.Static` / `Content.Adaptor` / `Content.Builder.Static` / `SkyContent.Builder`
- [x] 6.2 运行 `Aurora.CoreTest`(67) / `Aurora.AdaptorTest`(16) / `Aurora.CookTest`(44)（全绿；未新增 builder 单测）
