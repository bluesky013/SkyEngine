## Context

Change 1（`aurora-rdg-compiledgraph-variant`）已完成 `CompiledGraph` variant payload 结构，但 `SceneRasterPayload.items` 为空，`FullScreenPayload`/`ComputePayload`/`CopyBlitPayload`/`PresentPayload`/`CustomPayload` 无对应 builder/API。

## Goals / Non-Goals

**Goals:**

- `RenderGraph` 新增 `AddSceneRasterPass` / `AddFullScreenPass` / `AddCopyBlitPass` / `AddPresentPass` / `AddCustomPass`。
- 6 个 builder 类：`SceneRasterPassBuilder` / `FullScreenPassBuilder` / `ComputePassBuilder` / `CopyBlitPassBuilder` / `PresentPassBuilder` / `CustomPassBuilder`。
- `RDGGraph.h` 对应 pass data 结构。
- `Compile.cpp::ProduceCompiledGraph` 填充 `SceneRasterPayload.items`。
- `Execute.cpp::ExecutePasses` `switch (pass.type)` dispatch 到各 payload 的 emit 逻辑。
- `RDGContext` 新增 `GetResourceGroup()` / `SetResourceGroup()`。

**Non-Goals:**

- 不实现三层 ResourceGroup 的完整绑定（`globalResourceGroup` 定义但 executor 暂不绑定 set 0，后续 change）。
- 不实现反射自动映射 ResourceGroup 绑定（后续 change）。
- 不改 `aurora-resource-group` change 的 `ResourceGroup` 定义（只消费）。

## Decisions

### 1. builder 职责分离

builder 只负责「声明依赖 + 收集数据」，不直接操作 encoder。`SceneRasterPassBuilder.AddDrawItem(item)` 收集 `DrawItem` 到 `SceneRasterPassData.items`；`FullScreenPassBuilder.SetTechnique(pso)` 记录 PSO 到 `FullScreenPassData.pso`。

### 2. SceneRasterPayload.items 从 setup graph 拷贝

`ProduceCompiledGraph` 从 `SceneRasterPassData.items` 拷贝到 `SceneRasterPayload.items`（`TransientVector` 拷贝，arena 内分配）。

### 3. Execute.cpp switch dispatch

`ExecutePasses` 按 `pass.type` dispatch：

- `SCENE_RASTER`：`BeginRendering` → for item: `BindPipeline`/`BindResourceGroup(2, batchRG)`/`BindVB`/`BindIB`/`DrawIndexed` → `EndRendering`
- `FULLSCREEN`：`BindPipeline(pso)` → `BindResourceGroup(1, passRG)` → `Draw(3,0,1,0)`
- `COMPUTE`：`BindPipeline(pso)` → `BindResourceGroup(1, passRG)` → `Dispatch(groups)`
- `COPYBLIT`：`switch(kind)` → `CopyBuffer`/`CopyImage`/`...`
- `PRESENT`：无额外 encoder 操作
- `CUSTOM`：`fn(ctx, cmdBuf)`

### 4. RDGContext::GetResourceGroup

`RDGContext` 新增 `GetResourceGroup()` / `SetResourceGroup()`，executor 内部 wiring 设置当前 pass 的 ResourceGroup。

## Risks / Trade-offs

- **[SceneRasterPayload.items 拷贝开销]** `TransientVector<DrawItem>` 拷贝 = arena 内 memcpy，开销可接受。
- **[builder API 复杂度]** 6 个 builder 类增加 API 表面积。→ 缓解：builder 只声明依赖，不操作 encoder。

## Migration Plan

1. `RDGGraph.h`：6 个 pass data 结构。
2. `RenderGraphBuilder.h`：6 个 builder 类。
3. `RenderGraph.h` / `.cpp`：6 个 `AddXxxPass` API。
4. `Compile.cpp`：`ProduceCompiledGraph` 填充 `SceneRasterPayload.items`。
5. `Execute.cpp`：`switch (pass.type)` dispatch。
6. `RDGContext.h`：`GetResourceGroup()` / `SetResourceGroup()`。
7. 测试：`RDGTest.cpp` 适配新 API。
8. archive 本 change。
