## Context

`aurora-adaptor` 现有 `MaterialAssetData`（`MaterialPropertyData{name, type(0/1), float value[4], Uuid texture}` + 单个 `shader` Uuid）对应旧材质模型。当前 `Material` 已改为 typed 属性（`MaterialPropertyType = RgFieldType`）+ `MaterialTechnique{ techniqueTag; shader; MaterialRenderState }`，资产需对齐；`MaterialInstance`（父材质 + 覆盖）尚无资产。

shader 身份在引擎里是 `ShaderRef{ relativePath, entry }`（`aurora/shader/ShaderRef.h`，声明给 material/pass 用），解析成 RHI `Shader` 需经 `ShaderResolver`（在 shader/渲染层）。`MaterialTechnique.shader` 目前却是具体 `CounterPtr<Shader>`。

约束：`aurora/core` 不依赖 framework；adaptor 是唯一桥接层（可依赖 framework）。资产序列化沿用 `BinaryArchive`（`SaveValue/LoadValue` 支持算术、`enum`、`Flags`（`BinarySerializable`）与 `std::string`、`Uuid` 以字符串存）。

## Goals / Non-Goals

**Goals:**

- 材质资产与当前 `Material` 对齐（typed 属性 + technique + material render state）。
- 新增 `MaterialInstance` 资产（父材质 + 值/纹理覆盖）。
- 提供运行时构建路径，产出 `CounterPtr<Material>` / `CounterPtr<MaterialInstance>`。

**Non-Goals:**

- 不在 adaptor 引入 `ShaderResolver`/shader 编译；shader 只存 `ShaderRef`，具体解析归渲染层。
- 不改 RHI。
- 不做离线 cook（运行时加载）。

## Decisions

### D1: `MaterialTechnique.shader` 改为 `ShaderRef`

- **Rationale**：`ShaderRef`（路径 + entry）是引擎里 material/pass 引用 shader 的正式身份，且与 `ShaderResolver::Request{relativePath, entry}` 对齐；`CounterPtr<Shader>` 是解析后的具体对象，不该直接进资产。
- **Alternative**：存 shader 的 Uuid —— 需要新增 Shader asset 类型（不存在），且与 `ShaderRef` 注释冲突。
- **Consequence**：`Material.h` include `aurora/shader/ShaderRef.h`；不再需要 `aurora/rhi/Shader.h`。

### D2: technique 的 `MaterialRenderState` 内联、逐字段序列化

asset 每个 technique 内联存 `MaterialRenderState`（`DepthStencil`/`RasterState`/`blendStates`/`alphaToCoverage`），逐字段 `SaveValue`。

- **Rationale**：RHI 结构体含 padding(`BlendState::padding`) 与 bool，`memcpy` 会写入不确定字节（非确定性/不可移植）；逐字段用 `SaveValue`（`enum`/`Flags`/算术直接支持）干净且确定。
- **评估过的替代（hash 去重表）**：asset 内按确定性 hash 去重 state，或外部 state library。否决：state 很小、technique 少，去重收益可忽略；hash 不可逆（仍须存 state 定义）；要做跨材质共享就变成查表库 + 跨资产引用（与 spec 相冲）。运行时 PSO cache 是渲染层的事，届时由渲染层算 state hash。

### D3: 属性记录 typed：`{ name, type, value bytes, texture Uuid }`

uniform 存原始值字节（大小由 `SizeOfPropertyType(type)` 决定，含 `bool` 1 字节）；纹理存 `Uuid`。

- **Rationale**：`type`（`MaterialPropertyType`）决定 `value` 的解释；与 `Material` 的“size 由 type 派生”一致。
- **bool**：按 user 结论——属性 map 并不直接对应 shader uniform buffer（后续按 technique 重组），故 bool 就是一个 1 字节 value，不做特殊化。

### D4: 版本与版本守卫

`MaterialAssetData` / `MaterialInstanceAssetData` 均带 `CURRENT_VERSION`；`Load` 在版本不匹配时清空并记录日志（对齐 `ImageAssetData` / `MeshAssetData`）。

### D5: 运行时构建

```cpp
CounterPtr<Material>         CreateMaterialFromAsset(Device *device, const Asset<Material> &asset);
CounterPtr<MaterialInstance> CreateMaterialInstanceFromAsset(Device *device, const Asset<MaterialInstance> &asset);
```

- 纹理经 `AssetManager::Get()->LoadAsset<Texture>(uuid)` → `CreateTextureFromAsset(device, …)` → `CounterPtr<Texture>`。
- technique 只填 `ShaderRef`（+ tag + renderState）；具体 `Shader` 由渲染层用 `ShaderResolver` 解析。
- `CreateMaterialInstanceFromAsset` 经 `AssetManager` 解析父 `Material` asset 后建实例并应用覆盖。
- **adaptor 不引入 `ShaderResolver`**；需要 `Device`（纹理需要），`device` 为空或版本不匹配返回空。

## Risks / Trade-offs

- **[渲染层未落地]** 构建出的 `Material.technique.shader` 只是 `ShaderRef`，暂无消费者。→ 契约明确，解析归渲染层。
- **[纹理构建需 Device/AssetManager]** 无 Device 时构建返回空。→ 版本/设备守卫 + 日志。
- **[属性 vs cbuffer 布局]** 属性是“值 map”，与 cbuffer 不是一一对应，后续按 technique 重组（含 bool 处理）。→ 记录为后续（`aurora-renderer` / technique 重组）。
- **[序列化格式变更]** 旧 `MaterialAssetData` 布局不兼容。→ 以 `CURRENT_VERSION` 守卫。

## Migration Plan

1. core：`MaterialTechnique.shader` → `ShaderRef`；更新 `MaterialTest`。
2. adaptor：重写 `MaterialAsset.h`；新增 `MaterialInstanceAsset.h`；新增 `MaterialAsset.cpp`；更新 `AuroraReflection.cpp`；新增往返测试。
3. 重跑 cmake（GLOB）→ 构建 `Aurora.AdaptorTest` / `Aurora.CoreTest` → 运行。
4. Rollback：回退相关文件（无离线产物/ABI 迁移）。

## Open Questions

- 属性→cbuffer 的重组（含 bool）在哪一层做（renderer / technique 缓存）？
- 是否需要把 `MaterialRenderState` 的确定性 hash 落到资产以预热 PSO cache？
