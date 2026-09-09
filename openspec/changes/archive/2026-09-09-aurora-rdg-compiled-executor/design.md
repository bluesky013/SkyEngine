## Context

`ExecutePasses` 仍遍历 setup graph（`mPasses`/`mTopoOrder`/`mSceneRasterPasses`），`CompiledGraph` 产出但无人消费。`RDGBackend::Execute` 只是 `graph.ExecutePasses(cmdBuf)` 透传。

## Goals / Non-Goals

**Goals:**

- `ExecutePasses` 只读 `mCompiledGraph`：`CompiledPass[]`（topo 序、live only）+ 扁平 `barriers[]` + `resolvedImages/Buffers`。
- `CompiledGraph::finalBarrierOffset` 标记帧末 barrier 段起点。
- `ComputePayload` / `CopyBlitPayload` 增加 `executeFn`（legacy 过渡）。
- `AddSceneRasterPass` 移除 execute 参数。

**Non-Goals:**

- 不改 `RDGBackend` 接口（仍透传）。
- 不实现 COPYBLIT 的数据化 copy（executeFn fallback 保留）。
- 不做三层 ResourceGroup 的 Global（set 0）绑定（globalResourceGroup 字段已定义，绑定留后续）。

## Decisions

### 1. ExecutePasses 遍历 CompiledGraph

```cpp
for (const auto &cpass : mCompiledGraph->passes) {
    // barriers: barriers[cpass.barrierOffset .. +cpass.barrierCount)
    // switch (cpass.type) → 各 payload emit
}
// final: barriers[finalBarrierOffset .. end)
```

### 2. Compute/CopyBlit 的 executeFn 进 payload

`ComputePayload::executeFn` / `CopyBlitPayload::executeFn`（`std::function`，过渡期）。CompiledGraph 自洽，executor 不回查 setup graph。

### 3. SceneRaster 的 execute lambda 正式废弃

`AddSceneRasterPass(name, setup)`；execute 由 `items` 数据驱动。测试里空 lambda 移除。

### 4. resolved 表直接用 CompiledGraph 的

executor 的 `RDGContext` 从 `mCompiledGraph->resolvedImages/resolvedBuffers` 取，不再用 `mResolvedImages/mResolvedBuffers`。

## Migration Plan

1. `CompiledGraph.h`：`finalBarrierOffset`；`ComputePayload`/`CopyBlitPayload` 加 `executeFn`。
2. `Compile.cpp`：填充 `finalBarrierOffset`；compute/copyblit 的 executeFn 拷入 payload。
3. `Execute.cpp`：`ExecutePasses` 遍历 CompiledGraph。
4. `RenderGraph.h/.cpp`：`AddSceneRasterPass` 移除 execute 参数。
5. 测试适配 + 全绿。
6. archive。
