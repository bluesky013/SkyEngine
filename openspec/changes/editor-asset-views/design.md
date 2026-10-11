## Context

- framework:一个资产 = 一个 product blob(`Asset<T>::DataType` 整体序列化);`File` 可随机读(`IStreamArchive::GetStream()`)。
- shell 已有 per-type `AssetViewProviderRegistry` / `IAssetViewProvider`(`CreateContent`/`CreatePreview`)seam,但无人注册,默认只渲染 per-target cook 表单。
- aurora 无 `Skeleton`;`Skin` 为 mesh 侧(`aurora/resource/Skin.h`,`SkinnedMesh` 持 `CounterPtr<Skin>`)。
- 参照:USD crate(头 + 类别粗区 + 索引寻址 + payload)、glTF(JSON + buffers)、Unreal `FBulkData`。

## Goals / Non-Goals

**Goals:**

- 资产 = **header + bulk**(粗 section);header 常驻,大块按需读。
- 视图 = **每资产自定义**,靠一套注册机制,由各模块 / plugin 的 editor 实现。

**Non-Goals:**

- 不做通用反射资产视图;不做完整 USD 组合引擎;不做 preview;不改 RHI。

## Decisions

### D1: 资产 = header + bulk(粗 section,USD 式)

product = `[prologue(type/deps/codec)][headerBlock][bulkSize][bulk]`。

- `Asset<T>::DataType` = **header**:版本、类型元数据、**references**,以及**每资源的 offset/size**(存进各自 header 结构)。
- `bulk` = 一大块不透明字节;`AssetBase::AddBulk`(建侧追加)/ `ReadBulk(offset,size)`(读侧 seek `bulkStart`);**无名字表/TOC**。
- headerBlock 按 codec 压缩;bulk 不压缩。

### D2: references 与承载

跨资产引用(mesh→skeleton/materials、material→texture、clip→skeleton)= header 存 uuid + `AddDependencies`。

### D3: variant = cook target/bundle

每个 target/bundle = 一个 product(变体);视图按变体展示由自定义 provider 处理。

### D4: 各资产 header(常驻)/ bulk(按需)

| 资产 | header | bulk(offset/size 存 header) |
|---|---|---|
| **Mesh** | attributes/stride/counts/indexType/bounds/refs | `vertex` + `index` + `skin`(skin 收编,不再独立资产) |
| **Image** | w/h/format/mips + slice 表 | `pixels`(每 slice 相对 bulk) |
| **AnimationClip** | name/frameRate + channel 表 + ref | `channels`(times/keys) |
| **Skeleton**(独立共享) | boneData/refPos | — |
| Material / MaterialInstance / LodGroup / AnimationGraph | 全部 | — |

### D5: 视图 = 每资产自定义(注册机制)

- `AssetViewProviderRegistry`(跨 DLL)→ `IAssetViewProvider::CreateContent(uuid,type)`;**去掉 `CreatePreview`**。
- **实现落各模块 / plugin editor**:`engine/aurora/editor`、**新增 `engine/content/editor`**、其它 plugin editor。
- **默认回退**(无 provider):有 cook 设置 → per-target 表单;否则 **HIDDEN(不打开)**。

### D5.1: 视图层落地(seam + 设计缺口)

spec 的 `DATA view is reflection-driven` 要求 `DATA` 段是绑定到**已注册 view data** 的 `ReflectedFormView`(材质/材质实例这类异构属性列表才用自定义 provider)。

**已实现(seam + aurora 数据):**

- `engine/sandbox/core/editor/core/asset/AssetViewData.h`:`AssetViewData{ sky::Any + const TypeNode* }` + `IAssetViewDataProvider` + `AssetViewDataRegistry`(跨 DLL Singleton,UI-free)。
- `AssetViewerWidget`:DATA 段优先 provider content,其次查 `AssetViewDataRegistry` 构建 `ReflectedFormView`(读只读 MVP);`DeriveAssetViewFlags` 的 DATA 也计入 registry。
- `engine/aurora/editor`:新增 `AuroraAssetViews.{h,cpp}` —— 注册 Image/Mesh/Skeleton/LodGroup/Material/MaterialInstance 的反射 view struct + provider(只读);`Aurora.Render.Editor` 增链 `EditorCore`/`Framework`。

**模块激活缺口(已按 D5.2 解决):**`Aurora.Render.Editor`/`Content.Views` 原 `: AuroraModule`(Init 会建 device),不能直接列入 `modules_editor.json`;改为 registration-only(见 D5.2)。

### D5.2: 视图层落地(已实现,registration-only 入口)

- **纯注册模块**(不建 device):`AuroraEditorModule`/`ContentEditorModule` 改为 `IModule`(Init 只跑 `AuroraReflection`/`ContentReflection` + 注册 view data,不再 `Instance::Init`/`CreateDevice`,不继承 `AuroraModule`)。
- `configs/modules_editor.json` 增加 `Aurora.Render.Editor` + `Content.Views`。
- `engine/content/editor` 新 target `Content.Views`(SHARED,链 `Content.Adaptor`+`EditorCore`+`Framework`+`Core`)。
- 覆盖:aurora Image/Mesh/Skeleton/LodGroup/Material/MaterialInstance;content AnimationClip/AnimationGraph(均只读反射 view struct)。构建通过。

### D6: 编辑提交

统一「**改源 / import 设置 → 重 cook → 重载**」(可选共享 helper)。

### D7: Mesh 移除 meshlet

见 tasks 3。

## 已实现(代码)

- **D1 framework**:`Asset.h`(`bulk`/`bulkStart`/`productStream` + `AddBulk`/`ReadBulk`)、`Asset.cpp`、`AssetManager::SaveAsset/LoadAsset/DeserializeProduct` 写/读 header + bulk。`FrameworkTest` 119/119。
- **D7 meshlet 移除**:`MeshAsset` v5、`CookedMesh`/`MeshAssetWriter`/`AuroraMeshBuilder`/`MeshBuildConfig` 清理、删 `MeshletBuilder`。`CookTest` 43、`Adaptor` 16、`Core` 67。
- **D4 各资产进 bulk**:
  - **Image** v2:header 去 `rawData`、加 `dataSize`,slice 相对 bulk;builder 三路径 `CommitImageBulk`(含 `.image` 重 cook 读源 bulk);`CreateTextureFromAsset` 用 `ReadBulk`。
  - **Mesh** v5:header 加 vertex/index/skin section offset+size,数据进 bulk;skin 收编(删 `SkinAsset`/handler/`SkinAssetData`/mesh `skin` uuid;新增 `CreateSkinFromMeshAsset` 读 mesh bulk);`BulletPhysicsRenderModule` 改读 bulk。
  - **AnimationClip** v2:channel 表 offset/size,times/keys 进 bulk(源 `.clip` 走 `LoadSource`,产物走 `Load`/`Save` + `BuildBulk`);`CreateAnimationClipFromAsset` 用 `ReadBulk`。
  - **Skeleton** 保持独立共享。
- **D5 视图层(已实现)**:seam(`AssetViewData.h` registry)+ viewer 构建 `ReflectedFormView`;aurora(`AuroraAssetViews`)Image/Mesh/Skeleton/LodGroup/Material/MaterialInstance;content(`Content.Views`)AnimationClip/AnimationGraph;registration-only 模块 + `modules_editor.json` 接入(见 D5.1/D5.2)。**编辑提交**:`BUNDLE` cook 设置编辑 → `ApplyCookSettings`(sparse override)→ `AssetCookService::TriggerCook` → `Reload`;`DATA` 摘要 view struct 标 `READONLY`(只读 MVP,spec 允许)。
- **跨 DLL 幂等修复(顺带)**:新增 `sky::Once`(`core/environment/Once.h`,跨 DLL);`AuroraReflection`/`ContentReflection`/`RegisterImageCookSettings` 的 per-DLL `static bool` 守卫改为 `Once`;`ShaderCompilerFactory`/`PhysicsBackendRegistry`/`NetworkBackendRegistry` 的 per-DLL 局部 static 单例改为 `Singleton<T>`。约束写入根 `AGENTS.md` 与 `skyengine-coding-rules`(Rule 8)、`engine/aurora/AGENTS.md`。
- **bulk 往返验证**:
  - `FrameworkTest`:`BulkRoundTripTest`/`NoBulkRoundTripTest`(header 常驻、`ReadBulk` 按需;无 bulk 时 `HasBulk()==false`)。
  - `Aurora.AdaptorTest`:`ImageBulkRoundTrip`(slice 相对 bulk)、`MeshBulkAndSkinRoundTrip`(vertex/index 经 `ReadBulk`、`CreateSkinFromMeshAsset` 从 mesh bulk 重建 skin)。
  - 新增 `Content.AdaptorTest`:`ClipBulkRoundTrip`(`LoadSource`→`BuildBulk`→`SaveAsset`/`LoadAsset`→`CreateAnimationClipFromAsset` 经 `ReadBulk`)。

## 待做

- 各类型 DATA view 的**源写回**(改源文件 → 重 cook):当前 `DATA` 摘要为**只读**(spec MVP 允许);`BUNDLE` 的 import/cook 设置编辑已通(`ApplyCookSettings` → `TriggerCook` → `Reload`,5.3)。
- 视图注册 + 回退/HIDDEN 的自动化验证(6.2)。
- 最终全量构建 + 测试(6.3)。


## Open Questions

- bulk 卸载的所有权/时机。
- Image builder 多路径(precompressed / `.image` 重 cook / 主)搬 bulk 的统一处理。
