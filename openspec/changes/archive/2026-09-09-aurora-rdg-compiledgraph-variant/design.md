## Context

`aurora-rdg-pass-structure` change 规模过大（46 任务），拆分为三个小 change。本 change 是第一步：`CompiledGraph` variant payload 结构 + `Compile.cpp`/`Execute.cpp` 适配。

当前 `CompiledPass` 复制三个 `std::function` 字段，类型擦除 + 堆分配，executor 手工分支。需要改为 `std::variant` payload，为后续 builder/API 重构打底。

## Goals / Non-Goals

**Goals:**

- `CompiledPassType` 扩展为 6 种 pass 类型。
- `CompiledPass::payload` 改为 `std::variant`，删除三个 `std::function` 字段。
- 定义 `DrawItem` / `SceneRasterPayload` / `FullScreenPayload` / `ComputePayload` / `CopyBlitPayload` / `PresentPayload` / `CustomPayload`。
- `CompiledGraph::globalResourceGroup`（set 0）。
- `Compile.cpp::ProduceCompiledGraph` 构造 variant payload。
- `Execute.cpp::ExecutePasses` `switch (pass.type)` dispatch。

**Non-Goals:**

- 不实现新 pass 类型的 builder/API（`AddFullScreenPass` 等，后续 change）。
- 不实现 `SceneRasterPayload.items` 的填充（现有 raster pass 无 `AddDrawItem`，items 为空）。
- 不实现三层 ResourceGroup 的完整绑定（`globalResourceGroup` 定义但 executor 暂不绑定，后续 change）。
- 不改 `RDGGraph.h` / `RenderGraphBuilder.h` / `RenderGraph.h`（后续 change）。

## Decisions

### 1. variant payload 替代 std::function

`CompiledPass::payload` 改为 `std::variant<SceneRasterPayload, FullScreenPayload, ComputePayload, CopyBlitPayload, PresentPayload, CustomPayload>`。现有 raster pass 映射到 `SCENE_RASTER`，compute 映射到 `COMPUTE`，copy 映射到 `COPYBLIT`。

### 2. DrawItem 定义但暂不填充

`DrawItem` 结构定义（`pso`/`batchResourceGroup`/`vb`/`ib`/`DrawIndexedArgs`），但现有 raster pass 无 `AddDrawItem`，`SceneRasterPayload.items` 为空。后续 change 添加 builder 后填充。

### 3. globalResourceGroup 定义但暂不绑定

`CompiledGraph::globalResourceGroup` 定义（set 0），但 executor 暂不绑定（后续 change 完整实现三层 ResourceGroup 绑定）。

### 4. CustomPayload 保留 std::function

`CustomPayload` 保留 `std::function<void(RDGContext&, CommandBuffer&)>`，用于 device 扩展逃生门。

## Risks / Trade-offs

- **[现有 raster pass 无 DrawItem]** `SceneRasterPayload.items` 为空，executor 的 `SCENE_RASTER` 分支暂走旧逻辑（`BeginRendering` + `executeFn`）。→ 缓解：后续 change 添加 builder 后填充 items。
- **[globalResourceGroup 未绑定]** executor 暂不绑定 set 0。→ 缓解：后续 change 完整实现三层绑定。

## Migration Plan

1. 重构 `CompiledGraph.h`：`CompiledPassType` 扩展 + variant payload 结构定义。
2. 适配 `Compile.cpp::ProduceCompiledGraph`：构造 variant payload（raster→SCENE_RASTER，compute→COMPUTE，copy→COPYBLIT）。
3. 适配 `Execute.cpp::ExecutePasses`：`switch (pass.type)` dispatch（现有逻辑映射到对应 payload）。
4. 编译通过 + RDGTest 全绿。
5. archive 本 change。
