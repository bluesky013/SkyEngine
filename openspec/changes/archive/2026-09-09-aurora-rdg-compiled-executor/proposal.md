## Why

`aurora-rdg-compiledgraph-variant` + `aurora-rdg-pass-builder` 已完成 CompiledGraph variant 结构与 6 种 pass builder，但 `RenderGraph::ExecutePasses` 仍遍历 **setup graph**（`mPasses` / `mTopoOrder` / `mSceneRasterPasses`），后端 `RDGBackend::Execute` 只是透传。CompiledGraph 产出了但没人消费，违背「RDG 层只产数据，executor 只读 CompiledGraph」的设计。

## What Changes

- **BREAKING** `RenderGraph::ExecutePasses` 改为遍历 `mCompiledGraph`（`CompiledPass` + 扁平 barriers + resolved 表），不再触碰 setup graph。
- **新增** `CompiledGraph::finalBarrierOffset`（final barriers 在扁平数组中的起始偏移，区分 per-pass 段与帧末段）。
- **修改** `ComputePayload` / `CopyBlitPayload` 增加 `executeFn` 字段（legacy lambda 过渡期保留；SceneRaster 的 execute lambda 已被 AddDrawItem 数据路径取代，AddSceneRasterPass 的 execute 参数正式废弃）。
- **删除** `AddSceneRasterPass` 的 `execute` 参数（setup 即全部，execute 由 items 驱动）。
- executor emit 逻辑：
  - `SCENE_RASTER`：`BeginRendering` → 遍历 `items`：`BindResourceGroup(2, batchRG)` / `BindPipeline(pso)` / `BindIndexBuffer` / `DrawIndexed` → `EndRendering`
  - `FULLSCREEN`：`BindResourceGroup(1, passRG)` → `BindPipeline(pso)` → `Draw(3,0,1,0)` → `EndRendering`
  - `COMPUTE`：`BindPipeline(pso)` → `BindResourceGroup(1, passRG)` → `Dispatch(groups)` 或 executeFn fallback
  - `COPYBLIT`：executeFn fallback（v1，copy encoder API 数据化后续）
  - `PRESENT`：无 encoder 操作
  - `CUSTOM`：`fn(ctx, cmdBuf)`

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `aurora-rdg`: `ExecutePasses` 消费 `CompiledGraph` 而非 setup graph；`AddSceneRasterPass` 移除 execute 参数。

## Impact

- **受影响代码**：`engine/aurora/rhi/interface/include/aurora/rdg/CompiledGraph.h`、`RenderGraph.h`；`engine/aurora/rhi/interface/src/rdg/Compile.cpp`、`Execute.cpp`、`RenderGraph.cpp`；`engine/aurora/rhi/test/RDGTest.cpp`。
- **后端**：`RDGBackend::Execute` 仍透传 `graph.ExecutePasses(cmdBuf)`，接口不变。
- **测试**：`RDGTest.cpp` 的 `AddSceneRasterPass` 调用移除 execute lambda 参数。
