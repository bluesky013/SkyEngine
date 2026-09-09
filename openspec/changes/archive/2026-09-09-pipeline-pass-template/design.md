## Context

RDG 三步重构完成（CompiledGraph variant、6 种 pass builder、executor 消费 CompiledGraph）。本 change 在其上叠加两层：

1. **RDG 内建 queue**：SceneRasterPass 从平铺 items 变为多 queue 结构（参考旧 render RDG 的 `RasterQueue`：`{ passID, viewID, rasterID, renderItems, resourceGroup, sort }`）。
2. **pipeline template 层**：`engine/aurora/pipeline/` 新模块，PipelinePass 基类 + OpaquePass。

旧 render RDG（`sky::rdg`，PMR 版）为 legacy 参考，不改动。

## Goals / Non-Goals

**Goals:**

- SceneRasterPass 支持多 queue：每 queue 独立 items、queue 级 ResourceGroup、排序策略标记；RDG 按声明序执行 queue。
- builder 兼容：无 queue 参数的 `AddDrawItem(item)` 路由到默认 queue 0。
- `PipelinePass` 基类：持久资源（PSO/RG）由 pass 持有；`OnSetup` 一次性创建；`BuildRDG` 每帧构建；`OnSceneChanged` 显式重建。
- `OpaquePass` v1：color+depth attachment + 一个 opaque queue（FRONT_TO_BACK），无 scene 集成。

**Non-Goals:**

- 不做 scene/primitive/culling 集成（后续 change）。
- 不做 BloomPass（验证 fullscreen 链留后续；本 change 只 OpaquePass）。
- 不做 queue 内排序实现（sortPolicy 只是数据标记，排序由收集方完成）。
- 不动旧 render/core 的 legacy rdg。

## Decisions

### 1. queue 结构内建于 SceneRasterPass

```
SceneRasterPass
  └─ queues[]（声明序）
       ├─ name
       ├─ items[]（收集方排好序，RDG 保序）
       ├─ queueResourceGroup（queue 级 RG，绑 set 1；无则沿用 pass 级）
       └─ sortPolicy（NONE / FRONT_TO_BACK / BACK_TO_FRONT，纯数据标记）
```

executor：for queue → `BindResourceGroup(1, queueRG or passRG)` → for item → `BindResourceGroup(2, batchRG)` → draw。

set 语义：set 0 = Global（CompiledGraph 级），set 1 = Pass/Queue 级，set 2 = Batch（item 级）。queue RG 优先于 pass RG（queue 有则绑 queue 的）。

### 2. 排序不在 RDG 内做

`sortPolicy` 只是标记；排序由模板层 Collect 时完成（上层知道 view/深度语义）。RDG 只保序执行。这遵守「RDG 纯数据驱动，不擅自重排」的既定约束。

### 3. PipelinePass 基类三段式

```cpp
class PipelinePass {
    virtual void OnSetup(Device *device);        // 一次性：创建 PSO / ResourceGroup（持久）
    virtual void BuildRDG(RenderGraph &graph);   // 每帧：声明资源 + pass + queue + items
    virtual void OnSceneChanged();               // 场景变化：显式重建持久资源
};
```

持久资源存成员变量（`GraphicsPipelinePtr mPSO` / `ResourceGroup*`），BuildRDG 时传给 builder。RDG 层不缓存、不管理其生命周期。

### 4. 模块位置与命名

`engine/aurora/pipeline/`（include/src），CMake target `Aurora.Pipeline`（STATIC），链接 `Aurora.RHI` + `Core`。`engine/aurora/CMakeLists.txt` 加 `add_subdirectory(pipeline)`。命名空间 `sky::aurora`。

### 5. OpaquePass v1 骨架

```cpp
class OpaquePass : public SceneRasterPassTemplate {
    // OnSetup: 占位（PSO 创建等 shader 管线接入后补）
    // BuildRDG: CreateTexture(color/depth) + AddSceneRasterPass + AddQueue("opaque", FRONT_TO_BACK)
    // Collect: 空钩子（scene 集成后续）
};
```

## Risks / Trade-offs

- **[queue 使 SceneRasterPayload 变胖]** payload 内嵌 `TransientVector<SceneRasterQueue>`，每个 queue 又嵌 items vector。→ 缓解：全部 TransientVector（arena），variant 只存一层。
- **[默认 queue 兼容路径的隐式行为]** 无 queue 参数的 AddDrawItem 懒创建 queue 0，可能掩盖「忘建 queue」的错误。→ 缓解：文档注明；debug 下 queue 0 名为 "default"。
- **[模块耦合]** pipeline 模块链接 aurora RHI，与旧 render/core 并存互不 include。

## Migration Plan

1. RDG：queue 结构 + builder + compile + execute + 测试。
2. engine/aurora/pipeline 模块骨架（CMake + PipelinePass 基类）。
3. SceneRasterPassTemplate + OpaquePass。
4. AuroraTest 加 queue 测试 + OpaquePass smoke test。
5. archive。
