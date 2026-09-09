## Why

`pipeline-pass-template` 已完成 PipelinePass 基类与 OpaquePass 骨架，但 `Collect` 是空钩子——pass 还不知道怎么从场景收集 draw call。旧 render/core 有成熟的对接模型（`RenderSceneVisitor`：queue 的 `rasterID` technique tag 作过滤器，primitive 按 tag 持有 technique，`GatherRenderItem` 命中才 append），但那是 legacy PMR + 旧 RHI 类型，不可直接复用。需要在 aurora/pipeline 内新建 scene 抽象并打通「pass ↔ scene」收集链路，让 OpaquePass / 未来 ShadowPass 能从场景收集 opaque / shadow draw call。

## What Changes

### Part 1 — aurora scene 抽象（`aurora/pipeline/scene/`）

- **新增** `SceneView`：frustum + view 常量数据；`FrustumCulling(bounds)`；view 常量是 future global RG（set 0）的数据源。
- **新增** `RenderPrimitive`：geometry（vb/ib/offsets/args）+ worldBounds + `map<Name tag, TechniqueBinding>`；`TechniqueBinding = { pso, batchResourceGroup }`；`GatherRenderItem(context)`：tag 命中才 append `DrawItem`。
- **新增** `RenderScene`：primitives 注册/注销 + views 管理；v1 无遮挡剔除系统。

### Part 2 — queue technique tag（RDG 层）

- **新增** `SceneRasterQueue::techniqueTag`（`Name`，默认空）——纯数据字段，RDG 不消费，收集方用作过滤器。
- **新增** `SceneRasterPassBuilder::AddQueue(name, sortPolicy, tag)` 重载；旧两参重载保留（tag 为空）。

### Part 3 — 收集链路（pass 模板）

- `SceneRasterPassTemplate` 新增 `SetScene(RenderScene*)` / `SetView(SceneView*)`；`Collect(builder)` 默认实现：frustum cull（v1）→ 遍历 primitives → `GatherRenderItem({tag=queue.tag, view})` → `AddDrawItem(queue, item)` → 按 queue.sortPolicy 排序。
- `OpaquePass`：持有 main view；queue tag `"opaque"`，FRONT_TO_BACK 排序。

## Capabilities

### New Capabilities

- `aurora-scene-collect`: SceneView / RenderPrimitive / RenderScene 的 aurora 抽象与 pass 收集链路（tag 过滤、frustum culling、排序）。

### Modified Capabilities

- `aurora-rdg`: `SceneRasterQueue` 新增 `techniqueTag` 数据字段。
- `pipeline-pass-template`: `SceneRasterPassTemplate` 新增 scene/view 持有与 `Collect` 默认实现。

## Impact

- **aurora RDG**：`CompiledGraph.h`（SceneRasterQueue + tag）、`RenderGraphBuilder.h`、`RenderGraph.cpp`（AddQueue 重载）。
- **aurora pipeline**：新增 `scene/SceneView.h`、`scene/RenderPrimitive.h`、`scene/RenderScene.h`；`SceneRasterPassTemplate` 扩展；`OpaquePass` 接入。
- **测试**：mock primitives（多 tag）→ 断言 opaque queue 只收 opaque tag item、shadow tag 互不污染；frustum cull 剔除验证。
- **不影响**：旧 render/core legacy 不动。
