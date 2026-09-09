## Why

`aurora-rdg-pass-structure` change 规模过大（46 任务，涉及 6 个文件大改），拆分为三个小 change。本 change 是第一步：`CompiledGraph` variant payload 结构定义 + `Compile.cpp`/`Execute.cpp` 适配，不包含新 pass 类型的 builder/API。

当前 `CompiledPass` 复制三个 `std::function` 字段（`rasterExecuteFn`/`computeExecuteFn`/`copyExecuteFn`），类型擦除 + 堆分配，executor 手工分支。需要改为 `std::variant` payload，为后续 builder/API 重构打底。

## What Changes

- **BREAKING** `CompiledPassType` 扩展为 `{SCENE_RASTER, FULLSCREEN, COMPUTE, COPYBLIT, PRESENT, CUSTOM}`。
- **BREAKING** `CompiledPass::payload` 改为 `std::variant<SceneRasterPayload, FullScreenPayload, ComputePayload, CopyBlitPayload, PresentPayload, CustomPayload>`，删除三个 `std::function` 字段。
- **新增** `DrawItem`（`pso`/`batchResourceGroup`/`vb`/`ib`/`DrawIndexedArgs`）、`SceneRasterPayload`（`colors`/`depthStencil`/`items`）、`FullScreenPayload`（`pso`/`passResourceGroup`/`colors`）、`ComputePayload`（`pso`/`passResourceGroup`/`groups`）、`CopyBlitPayload`（`kind`/`src`/`dst`）、`PresentPayload`（`image`）、`CustomPayload`（`std::function`）。
- **新增** `CompiledGraph::globalResourceGroup`（set 0，pipeline 全局一个）。
- **适配** `Compile.cpp::ProduceCompiledGraph` 构造 variant payload（现有 raster/compute/copy pass 映射到 `SCENE_RASTER`/`COMPUTE`/`COPYBLIT`）。
- **适配** `Execute.cpp::ExecutePasses` `switch (pass.type)` dispatch（现有逻辑映射到对应 payload 的 emit）。

## Capabilities

### New Capabilities

- `aurora-rdg-compiledgraph-variant`: `CompiledPass` variant payload 结构（三层 ResourceGroup + DrawItem + 6 种 pass 类型）。

### Modified Capabilities

- `aurora-rdg`: `CompiledPass` 重构（variant 替代 std::function）；`ProduceCompiledGraph`/`ExecutePasses` 适配。

## Impact

- **受影响代码**：`engine/aurora/rhi/interface/include/aurora/rdg/CompiledGraph.h`、`engine/aurora/rhi/interface/src/rdg/Compile.cpp`、`Execute.cpp`。
- **外部调用方**：RDG 仅测试调用，无外部调用方受影响。
- **后续 change**：`aurora-rdg-pass-builder`（builder + AddXxxPass API）依赖本 change 的 variant payload 结构。
