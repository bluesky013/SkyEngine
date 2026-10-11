## Why

1. **资产 = header + bulk**:一个资产 = 一个 product blob,mesh/image 即使只用 header 也会加载整块数据。应像 USD crate:header 常驻,大块按需读。
2. **视图**:shell 有 per-type `IAssetViewProvider` seam 但无人注册,默认只渲染一堆 target 表单。应改为**每资产自定义**,由各模块 / plugin 的 editor 实现。
3. **Mesh 移除 meshlet**;skin 收进 mesh。

## What Changes

### A. 资产 = header + bulk(USD 式,粗 section)
- product = `[prologue][headerBlock][bulkSize][bulk]`;`Asset<T>::DataType` = header(metadata + references + 每资源 offset/size);bulk 一大块按需读(`AddBulk`/`ReadBulk`);无名字表/TOC。
- 仅 **Mesh / Image / AnimationClip** 有 bulk;skin 收进 mesh 的 bulk;Skeleton 独立共享。

### B. 视图 = 每资产自定义
- `AssetViewProviderRegistry` → `IAssetViewProvider::CreateContent`;**去掉 `CreatePreview`**。
- 实现在各模块 / plugin editor:`engine/aurora/editor`、**新增 `engine/content/editor`**。
- 默认回退(无 provider):有 cook 设置 → per-target 表单;否则 **HIDDEN**。
- **不做**通用反射视图 / `Blob` kind / data-view 注册。

### C. Mesh 移除 meshlet
- `MeshAssetData` 删 meshlet 四段 + 结构/builder/config/测试;版本 bump。

## Capabilities

### Modified
- `aurora-adaptor` / `aurora-mesh-cook` / `content`:资产 header + bulk;Mesh 去 meshlet;skin 收编。
- `editor-asset-viewer`:视图改每资产自定义(注册机制,去 `CreatePreview`);默认回退 / `HIDDEN`。

## Impact

- Framework:`Asset`/`AssetManager` 产品格式(header + bulk)。
- aurora:`adaptor/assets/{MeshAsset,ImageAsset}`、`cook/{mesh,image}`。
- content:`adaptor/assets/AnimationAsset`、`builder/AnimationBuilder`。
- Editor:`editor/shell/AssetViewProvider`、`engine/aurora/editor`、新增 `engine/content/editor`、`SandboxModule`。
