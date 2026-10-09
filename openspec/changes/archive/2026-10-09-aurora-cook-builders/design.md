## Context

aurora cook（`engine/aurora/cook`）现有 `AuroraImageBuilder` / `AuroraMeshBuilder`，注册在 `AuroraCookModule`（`AssetBuilderManager::RegisterBuilder`）。`AssetBuilder` 契约：`GetExtensions()` / `QueryType(ext)` / `LoadConfig(fs)` / `Request(AssetBuildRequest, AssetBuildResult)`。`AssetBuildRequest` 含 `file`（源）、`assetInfo`（`AssetSourceInfo{ path, ext, uuid, dependencies }`）、`target`。引用别的源资产用 `AssetDataBase::RegisterAsset(path)`（返回带 `uuid` 的 `AssetSourcePtr`）并登记依赖。

旧引擎参照：`MaterialBuilder`（`.mat`/`.mati`，rapidjson 解析 `techniques` + `properties`）、`LodGroupBuilder`、`SkeletonBuilder`。注意 aurora 已把 technique 内联进 `MaterialTechnique`（无顶层 Technique 资源），故 `.mat` 需内联 technique。

目标产物结构（`aurora/adaptor/assets`）：
- `MaterialAssetData { version; techniques{MaterialTechniqueData{tag, ShaderRef, MaterialRenderState}}; properties{MaterialPropertyData{name, type, value bytes, texture Uuid}} }`
- `MaterialInstanceAssetData { version; material Uuid; valueOverrides; textureOverrides }`
- `LodGroupAssetData { version; levels{LodGroupLevelData{screenSize, mesh Uuid}} }`

约束：cook 不引入新三方库（JSON 走框架 `rapidjson`）；`AuroraCook.Static` 链接 `Aurora.Adaptor.Static` + `Framework`。

## Goals / Non-Goals

**Goals:**

- 为 material / materialinstance / lodgroup 提供源→产物的离线 builder，产物即现有 asset data。
- 与旧引擎的源扩展名/语义对齐（`.mat`/`.mati`/`.lod`）。

**Non-Goals:**

- 不新增 shader 资产或做 shader 编译（shader 以 `ShaderRef` 路径存）。
- 不单列 skin builder（skin 保持 mesh cook 产物）。
- 不改 RHI；`aurora/core` 不定义 Skeleton（Skeleton 桥接只在 adaptor）。

## Decisions

### D1: 扩展名 `.mat` / `.mati` / `.lod`

沿用旧引擎扩展名。`AuroraMaterialBuilder` 声明 `.mat`，`AuroraMaterialInstanceBuilder` 声明 `.mati`，`AuroraLodGroupBuilder` 声明 `.lod`；`QueryType` 分别返回 `AuroraMaterial` / `AuroraMaterialInstance` / `AuroraLodGroup`。

### D2: `.mat` 内联 technique，shader 存 `ShaderRef`

```json
{ "techniques": [ { "tag": "opaque", "shader": "shaders/pbr.slang", "entry": "main", "renderState": { ... } } ] }
```

`shader` → `ShaderRef{ relativePath = 路径, entry = "entry" or "main" }`。**不解析 shader、不登记 shader 依赖**（shader 非资产，解析归渲染层 `ShaderResolver`）。

### D3: `renderState` 逐字段可选

`renderState` 为对象，缺省键用 `MaterialRenderState` 默认值；解析常见子集：

- `depthTest` / `depthWrite` / `depthCompare`（`CompareOp` 名）/ `stencilTest`：折入 `depthStencil`。
- `cullMode`（`none`/`front`/`back`/`both`）、`frontFace`（`cw`/`ccw`）、`polygonMode`（`fill`/`line`/`point`）、`depthBias`（可选对象）：折入 `rasterState`。
- `alphaToCoverage`（bool）。
- `blend`（单个对象或数组）：`{ "enable", "srcColor", "dstColor", "srcAlpha", "dstAlpha", "colorOp", "alphaOp" }`（名为 `BlendFactor`/`BlendOp`）→ `blendStates`（数组按 color target）。

### D4: 属性 JSON 推断（参考 Unity/Unreal 约定）

`properties` 对象内按值类型推断（可选 `{ "type": ..., "value": ... }` 显式覆盖）：

| JSON | 类型 | 字节 |
|---|---|---|
| `number`（int/uint/浮点） | `FLOAT` | 4 |
| `bool` | `BOOL` | 1 |
| `[2]` / `[3]` / `[4]` | `FLOAT2/3/4` | 8/12/16 |
| `string` | 纹理 `Uuid` → `TEXTURE2D` | — |
| `{ "type": "INT"/"UINT", "value": n }` | `INT`/`UINT` | 4 |
| `{ "type": "MAT4", "value": [16] }` | `MAT4` | 64 |

值为原始字节（`AddValue` 语义一致）。

### D5: 引用统一用 Uuid，Shader 例外

**资产引用一律 `Uuid`**（源里存 uuid 字符串）：`texture` / `material` / `mesh` / `skeleton`。**唯一例外是 Shader**（`ShaderRef` = 路径 + entry，非资产）。

builder 解析：`const Uuid ref = Uuid::CreateFromString(str);`

```cpp
asset->AddDependencies(ref);                    // product 依赖（始终）
if (AssetDataBase::Get()->Exists(ref)) {        // 是已注册源才建 source 边
    request.assetInfo->dependencies.emplace_back(ref);
}
```

- **Rationale**：统一后不区分"路径 vs 已有 Uuid"，同一套逻辑；`Exists(uuid)` 判断是否源，避免对 mesh 产物（无源）误建 source 边。
- **顺带解决**：`.clip` 的 skeleton 之前因"二进制里是 Uuid、无法 `RegisterAsset(path)`"只能写 product 依赖；统一后只要它是已注册源就能写 source 依赖，实现同一套逻辑（见 D10 相关说明）。
- **后果**：现有 `assets/materials/*.mat` 用**路径**引用（且是旧 `.tech` 格式），统一到 uuid 后旧源会解析失败，需由编辑器/导入器重出。

### D5b: Shader 例外（`ShaderRef`）

`.mat` 内联 technique 的 `shader` 仍是 `ShaderRef`（路径 + entry）：shader 不是资产、由 `ShaderResolver` 按路径解析，故不参与 uuid 引用与依赖。

### D6: skin 不单列 builder

skin 由 `AuroraMeshBuilder` 在 `config->skinning && source.Skinned()` 时产出（`WriteSkinAsset`）。本 change 不为 skin 定义源格式或 builder。

### D7: 注册

`AuroraCookModule::Init` 追加 `RegisterBuilder(new AuroraMaterialBuilder())` / `...MaterialInstanceBuilder()` / `...LodGroupBuilder()`。cook `CMakeLists.txt` 的 GLOB 追加 `material/src/*`、`lodgroup/src/*` 与对应 include。

### D8: Skeleton 资产桥接（不违反 "aurora 无 Skeleton"）

`aurora/core` 不定义 `Skeleton`（`aurora/AGENTS.md`）；`Skeleton` 属 `engine/animation`。故 `SkeletonAssetData` / `AssetTraits<sky::Skeleton>` 放在 **adaptor 桥接层**，`Aurora.Adaptor.Static` 链接 `Animation`。`FetchOrCreateSkeletonByAsset` 用 `Skeleton::BuildSkeleton`。

- **Alternative**：把 Skeleton 放进 aurora core —— 违反 AGENTS，且给 core 引入 Animation 依赖。
- **Serialization**：`SkeletonAssetData` 逐字段（`boneData` 的 `Name` + `parentIndex`，`refPos` 的 `Transform`；`Name` 存字符串）。

### D9: Mesh builder 产出 skeleton

`MeshAssetData` 增 `Uuid skeleton`（版本 2 → 3）。蒙皮源下 mesh cook 由节点层级推导骨骼（`parent` + 局部参考变换，索引与 `SkinAssetData::boneNames` 对齐），额外写一个 `Skeleton` 资产并把其 Uuid 写入 mesh 数据、登记依赖；非蒙皮源 `skeleton` 为空。推导用 assimp `aiMatrix4x4::Decompose`（经 `core/math/MathUtil.h` 的 `Decompose`）。

### D10: 独立 `engine/content` 模块承载非渲染域资产

动画资产不适合放 `render`（域不对）或 `aurora`（animaion 非渲染域），也不能放 `engine/animation` core（会引入 `Framework` 依赖，违反 "`Animation` 只依赖 `Core`"）。故新增独立模块 `engine/content`，按「特性 = core + adaptor + builder」分层：`Content.Adaptor`（链 `Animation` + `Framework`）放资产数据/`AssetTraits`/reflection；`Content.Builder.Static` + `SkyContent.Builder` module 放 builder。`engine/animation` 的 `Animation` 保持框架无关。后续 prefab 等非渲染域资产的桥/builder 也归此模块。

### D11: 公共 builder helper 落到 framework `AssetBuilder`

五个 JSON builder（material / materialinstance / lodgroup / skeleton / animation-graph）曾逐字重复「空文件检查 → ReadString → rapidjson 解析 → FindOrCreateAsset → 依赖 → SaveAsset」。抽象为 framework `AssetBuilder` 的 protected helper（中立层，aurora cook 与 content 共用）：`LoadJsonDocument` / `CreateProduct<T>` / `AddAssetDependency`（product+source 依赖统一，`Exists` 判定）/ `FinishProduct`。同时统一 `Uuid` 序列化为二进制 `SaveValue(uuid)`（去掉 `ToString/CreateFromString` 往返），并把 `{name,type,value-bytes}` 编解码抽为 `detail::SaveTypedValue/LoadTypedValue`。

### D12: content 拆两个 module

`Content.Module`（runtime，只注册 data/handler）+ `Content.Builder.Module`（cook，注册 builder），对齐「运行时 vs 工具」两条加载路径。

## Risks / Trade-offs

- **[renderState 子集非全量]** stencil 细节/多 target 复杂 blend 未来可能需扩充。→ 键可选、向后兼容；缺省即默认。
- **[路径解析依赖 AssetDataBase 已初始化]** cook 环境已具备（与 image/mesh 一致）。→ 与既有 builder 同假设。
- **[跳过缺失引用]** 引用解析失败记警告并继续（不中断整体构建），与旧引擎一致。

## Migration Plan

1. 新增 `cook/material/**`（builder + 解析）、`cook/lodgroup/**`（builder）。
2. 更新 cook `CMakeLists.txt`（GLOB + include）。
3. `AuroraCookModule` 注册三者。
4. 重跑 cmake → 构建 `Aurora.Cook` / `AuroraCook.Static`（+ `Aurora.CookTest`）。
5. Rollback：删除新增目录 + 撤销注册（无产物迁移）。

## Open Questions

- 是否需要 `.mat` 的 technique 也支持引用“shader 变体开关”？（当前 shader 仅路径 + entry）
- 是否补 skin 的独立源（旧引擎 SkeletonBuilder 对应）？
