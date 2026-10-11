## 1. Framework：header + bulk ✅

- [x] 1.1 product 格式 `[prologue][headerBlock][bulkSize][bulk]`（粗 section，无名字表）
- [x] 1.2 `AssetHandlerBase::Save/Load` 只读 header；header 结构带 offset/size
- [x] 1.3 `AssetBase::AddBulk` / `ReadBulk(offset,size)`（seek `bulkStart`；有 bulk 才保留产品流）

## 2. 资产进 bulk（待做）

- [x] 2.1 **Image**：header 去 `rawData`、加 `dataSize`；`slice.offset` 相对 bulk；builder 各 SaveAsset 路径前 `AddBulk` 清 `rawData`（`.image` 重 cook 路径读源 bulk）；`CreateTextureFromAsset` 用 `ReadBulk`；版本 1→2
- [x] 2.2 **Mesh**：header 加 `vertex/index/skin` offset+size；数据进 bulk；**skin 收编**（删 `AssetTraits<Skin>`/handler/`SkinAssetData`/mesh `skin` uuid；`SkinnedMesh` 读 mesh bulk）；版本 4→5
- [x] 2.3 **AnimationClip**：channel 表加 offset/size；times/keys 进 bulk；`CreateAnimationClipFromAsset` 用 `ReadBulk`；版本 bump
- [x] 2.4 Skeleton 保持独立共享（不拆）

## 3. Mesh 移除 meshlet ✅

- [x] 3.1 `MeshAssetData` 删 meshlet 四段 + 结构 + `HasMeshlets`；版本 3→4
- [x] 3.2 `CookedMesh`/`MeshAssetWriter`/`AuroraMeshBuilder`/`MeshBuildConfig` 删 meshlet；删 `MeshletBuilder.{h,cpp}`；`MeshCookTest` 清理

## 4. 视图：注册机制 + 默认回退（待做）

- [x] 4.1 `IAssetViewProvider` 去 `CreatePreview`；preview 只留 `IAssetPreviewProvider` 占位
- [x] 4.2 默认回退：有 cook 设置 → per-target 表单；否则 `HIDDEN`（`SandboxModule` 不打开）

## 5. 各模块 editor 实现（待做）

- [x] 5.1 `engine/aurora/editor`：Mesh/Image/Material/MaterialInstance/Skeleton/LodGroup 视图
- [x] 5.2 **新建 `engine/content/editor`**：AnimationClip/Graph 视图
- [x] 5.3 编辑提交：改源/import 设置 → 重 cook → 重载

## 6. 验证

- [x] 6.1 各资产 bulk 往返（只读 header 不加载 bulk；bulk 按需）
- [x] 6.2 视图注册 + 回退/HIDDEN
- [x] 6.3 构建 + `FrameworkTest` / `Aurora.*Test` / `EditorShellTest`
