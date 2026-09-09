## Why

RDG 三步重构（compiledgraph-variant / pass-builder / compiled-executor）已完成，但 RDG 只提供裸 pass 原语。上层渲染需要一个 **pipeline template 层**：封装具体 pass（OpaquePass、BloomPass 等），由 pass 自己持有持久资源（PSO / ResourceGroup），每帧构建 RDG。同时 SceneRasterPass 需要**内建 queue 概念**：一个 pass 内多个 queue（如 opaque / transparent / shadow），各 queue 有独立 items 列表、queue 级 ResourceGroup、排序策略，RDG 编译/执行感知 queue。

## What Changes

### Part 1 — RDG 内建 queue（aurora 层）

- **BREAKING** `SceneRasterPassData::items` 改为 `queues: TransientVector<SceneRasterQueue>`；`SceneRasterQueue = { name, items, queueResourceGroup, sortPolicy }`。
- **新增** `QueueSortPolicy` 枚举：`NONE / FRONT_TO_BACK / BACK_TO_FRONT`（纯数据标记，排序由收集方在填充时完成，RDG 保序执行）。
- **新增** `SceneRasterPassBuilder::AddQueue(const Name&) -> uint32_t` / `AddDrawItem(uint32_t queue, const DrawItem&)` / `SetQueueResourceGroup(uint32_t queue, ResourceGroup*)`。
- **兼容** `AddDrawItem(item)`（无 queue 参数）路由到默认 queue 0（懒创建）。
- `SceneRasterPayload::queues`（替代平铺 `items`）；executor 按声明序遍历 queue：绑定 queue RG（set 1）→ 遍历 items（set 2 batch RG + draw）。

### Part 2 — pipeline template 层（engine/aurora/pipeline/ 新模块）

- **新增** `PipelinePass` 基类：`OnSetup(Device*)`（一次性创建 PSO/RG，持久）/ `BuildRDG(RenderGraph&)`（每帧构建）/ `OnSceneChanged()`（显式重建持久资源）。
- **新增** `SceneRasterPassTemplate : PipelinePass`：声明 queue（`AddQueue(name, sortPolicy)`），`Collect` 钩子留给子类/未来 scene 集成。
- **新增** `OpaquePass`：color + depth attachment，一个 `opaque` queue（FRONT_TO_BACK），v1 无 scene 集成（Collect 为空钩子）。
- **新增** CMake 模块 `engine/aurora/pipeline/`，target `Aurora.Pipeline`，链接 `Aurora.RHI` + `Core`。

## Capabilities

### New Capabilities

- `pipeline-pass-template`: PipelinePass 基类 / SceneRasterPassTemplate / OpaquePass 的契约（持久资源管理、每帧 BuildRDG、queue 声明）。

### Modified Capabilities

- `aurora-rdg`: SceneRasterPass 内建 queue（多 queue、queue 级 ResourceGroup、排序策略标记）；builder/compile/execute 适配。

## Impact

- **aurora RDG**：`RDGGraph.h`（SceneRasterPassData）、`RenderGraphBuilder.h`、`RenderGraph.h/.cpp`、`Compile.cpp`、`Execute.cpp`、`RDGTest.cpp`。
- **新模块**：`engine/aurora/pipeline/`（include/src + CMakeLists），`engine/aurora/CMakeLists.txt` 加 `add_subdirectory(pipeline)`。
- **测试**：RDGTest 加 queue 测试；OpaquePass smoke test 放 AuroraTest（链接 RenderPipeline）。
- **不影响**：旧 render/core 的 legacy rdg（`sky::rdg`）不动。
