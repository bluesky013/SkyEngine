# aurora-renderpass Specification

## Purpose
TBD - created by archiving change aurora-renderpass-begin-end. Update Purpose after archive.
## Requirements
### Requirement: Render pass begin/end 声明契约

Pipeline 层 pass SHALL 通过 color attachment（槽位 + handle + load/store + clear）与 depth/stencil attachment（handle + depth/stencil load/store + clear）以及 render area 声明 render target，**不**手写 `BeginRendering`/`EndRendering`。RDG executor SHALL 在编译后为 SCENE_RASTER 与 FULLSCREEN pass 统一生成 `BeginRendering(RenderingInfo)` 与 `EndRendering()`。

#### Scenario: SceneRasterPass 生成 begin/end

- **WHEN** 一个 `SceneRasterPassTemplate` 子类的 `BuildRDG` 声明了 color + depth attachment 并 `Compile()` + `Execute()` 后
- **THEN** executor 在遍历该 pass 的 queue items 之前调用 `BeginRendering`，在之后调用 `EndRendering`，且 `RenderingInfo` 的 attachment 数量、load/store 与声明一致

### Requirement: ScenePass (HDR)

`ScenePass` SHALL 继承 `SceneRasterPassTemplate`，在 `BuildRDG` 中声明一个 HDR color target（`RGBA16_SFLOAT`，`RENDER_TARGET | TRANSFER_SRC` usage）+ depth target（`D32`，`DEPTH_STENCIL` usage），color 与 depth 均以 `CLEAR` load，并声明一个名为 `opaque` 的 queue（`FRONT_TO_BACK`）。`ScenePass` SHALL 暴露 `GetHDRColorHandle()` 与 `GetDepthHandle()`，并提供 `SetExtent(width, height)`。

#### Scenario: ScenePass 构建 HDR RDG 节点

- **WHEN** `scenePass.SetExtent(1280, 720); scenePass.BuildRDG(graph); graph->Compile()`
- **THEN** compiled graph 中存在一个 `SCENE_RASTER` pass，其 color attachment 为 HDR 格式且 loadOp 为 `CLEAR`，depth attachment loadOp 为 `CLEAR`，且 queues 含名为 `opaque` 的 queue（`FRONT_TO_BACK`）

### Requirement: TextureToScreenPass

`TextureToScreenPass` SHALL 继承 `PipelinePass`，在 `BuildRDG` 中声明一个 FullScreenPass：以 `SetInputSRV(inputHandle)` 建立对输入 HDR texture 的 SRV read 依赖，以 `SetTarget(outputHandle, DONT_CARE, STORE)` 声明输出 color attachment（全屏覆盖）。`TextureToScreenPass` SHALL 暴露 `SetInput(RDGTextureHandle)` 与 `SetOutput(RDGTextureHandle)`。

#### Scenario: TextureToScreen 构建 fullscreen 节点

- **WHEN** `pass.SetInput(hdr); pass.SetOutput(backbuffer); pass.BuildRDG(graph); graph->Compile()`
- **THEN** compiled graph 中存在一个 `FULLSCREEN` pass，其 color attachment 指向 output 且 loadOp 为 `DONT_CARE`，并且输入 HDR texture 与该 pass 存在 SRV read 依赖（供 barrier/culling 推导）

#### Scenario: 输入 HDR texture 从 RTV 转 SRV

- **WHEN** `ScenePass` 写出 HDR color 后由 `TextureToScreenPass` 以 SRV 读取，且 RDG `Compile()` 完成
- **THEN** 两 pass 之间对 HDR texture 产生从 RTV 到 SRV 的 barrier（由 `SetInputSRV` 的 read 依赖驱动），保证先写后读

