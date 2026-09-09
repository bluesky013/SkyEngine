## Why

Change 1（`aurora-rdg-compiledgraph-variant`）已完成 `CompiledGraph` variant payload 结构，但 `SceneRasterPayload.items` 为空（现有 raster pass 无 `AddDrawItem`），`FullScreenPayload`/`ComputePayload`/`CopyBlitPayload`/`PresentPayload`/`CustomPayload` 无对应 builder/API。需要补全 builder + `AddXxxPass` API，让上层能构建 6 种基础 pass。

## What Changes

- **BREAKING** `RenderGraph::AddRasterPass` 更名为 `AddSceneRasterPass`；新增 `AddFullScreenPass` / `AddCopyBlitPass` / `AddPresentPass` / `AddCustomPass`。
- **新增** `SceneRasterPassBuilder`（`ColorAttachment` / `DepthStencilAttachment` / `AddDrawItem`）、`FullScreenPassBuilder`（`SetTechnique` / `SetPassResourceGroup` / `SetTarget` / `SetDepthStencil` / `SetInputSRV`）、`ComputePassBuilder`（`SetPipeline` / `SetPassResourceGroup` / `SetGroups` / `Read` / `Write`）、`CopyBlitPassBuilder`（`Src` / `Dst` / `SetKind` / `SetSize` / `SetOffsets`）、`PresentPassBuilder`（`SetSource`）、`CustomPassBuilder`（`SetCallback`）。
- **新增** `RDGGraph.h` 对应 pass data 结构：`SceneRasterPassData` / `FullScreenPassData` / `ComputePassData` / `CopyBlitPassData` / `PresentPassData` / `CustomPassData`。
- **新增** `RDGContext::GetResourceGroup()` / `SetResourceGroup()`（executor 内部 wiring）。
- **适配** `Compile.cpp::ProduceCompiledGraph` 填充 `SceneRasterPayload.items`（从 `SceneRasterPassData.items` 拷贝）。
- **适配** `Execute.cpp::ExecutePasses` `switch (pass.type)` dispatch 到各 payload 的 emit 逻辑（固定 set index 绑定）。

## Capabilities

### New Capabilities

- `aurora-rdg-pass-builder`: 6 种基础 pass 的 builder + `AddXxxPass` API。

### Modified Capabilities

- `aurora-rdg`: `RenderGraph` 新增 pass 类型注册接口；`RDGGraph.h` 新增 pass data 结构；`Compile.cpp`/`Execute.cpp` 适配 variant payload。
- `aurora-rdg-handles`: `RDGContext` 新增 `GetResourceGroup()` / `SetResourceGroup()`。

## Impact

- **受影响代码**：`engine/aurora/rhi/interface/include/aurora/rdg/RDGGraph.h`、`RenderGraph.h`、`RenderGraphBuilder.h`、`RDGContext.h`；`engine/aurora/rhi/interface/src/rdg/Compile.cpp`、`Execute.cpp`、`RenderGraph.cpp`；`engine/aurora/rhi/test/RDGTest.cpp`。
- **外部调用方**：RDG 仅测试调用，无外部调用方受影响。
- **后续 change**：新 pass 类型测试（FullScreen/CopyBlit/Present/Custom）。
