## Why

RDG 的 executor 已能依据「color/depth attachment + load/store」为每个 pass 生成 `BeginRendering`/`EndRendering`（Vulkan dynamic rendering），但 pipeline 层目前只有 `OpaquePass`（LDR `RGBA8_UNORM`）一个示范，缺少对 renderpass begin/end 的显式设计与「场景 → HDR → 屏幕」的最小帧管线。要推进真正的帧渲染，需从 pipeline 层补充 HDR 场景 pass 与合成到屏幕的 pass，并把 renderpass begin/end 的声明契约固定下来。

## What Changes

- **新增 `ScenePass`（HDR）**：`SceneRasterPassTemplate` 子类，声明 HDR color target（`RGBA16_SFLOAT`）+ depth（`D32`），CLEAR load，一个 `opaque` queue（`FRONT_TO_BACK`）。
- **新增 `TextureToScreenPass`**：`PipelinePass` 子类（fullscreen），输入 HDR texture（SRV read），输出 backbuffer/swapchain（imported texture）作为 color attachment；全屏三角形绘制。v1 直通拷贝，tonemap 留后续。
- **固化 renderpass begin/end 声明模型**：pass 通过 color/depth attachment + load/store/clear 声明 render target；RDG executor 据此生成 `BeginRendering`/`EndRendering`。此为 pipeline 层契约的固化（现有行为），不改 RHI 接口。
- 保留 `OpaquePass` 不动（demo 用途）。

## Capabilities

### New Capabilities

- `aurora-renderpass`: renderpass begin/end 声明契约 + 具体 pass（`ScenePass`(HDR) / `TextureToScreenPass`）的行为与场景。

### Modified Capabilities

（无 —— 本 change 只新增能力，不改动既有 spec 级需求；`pipeline-pass-template` 的 `OpaquePass` 保留不动。）

## Impact

- **新文件**：`engine/aurora/pipeline/include/aurora/pipeline/ScenePass.h`、`TextureToScreenPass.h`；`engine/aurora/pipeline/src/ScenePass.cpp`、`TextureToScreenPass.cpp`。
- **CMake**：`engine/aurora/pipeline/CMakeLists.txt` 用 `GLOB_RECURSE`，新增源文件自动纳入，无需改动。
- **测试**：`AuroraPipelineTest` 增加 `ScenePass` / `TextureToScreenPass` 的 BuildRDG smoke test。
- **不影响**：RHI / RDG 接口层、旧 render/core legacy、`OpaquePass` 现有行为。
