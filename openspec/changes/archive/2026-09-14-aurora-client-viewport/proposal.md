## Why

Aurora 的 Queue / Submit / SwapChain / Semaphore / Fence 已落地（`aurora-queue-submit-present`），RDG 也已具备 Import 与 PresentPass（PRESENT barrier/culling 种子）。但中间缺一层「可渲染表面」抽象：谁持有 SwapChain、驱动 Acquire → 渲染 → Present，并把 backbuffer 喂给 RDG。

现有 `RenderViewport`（`Acquire()/Release()` 空接口）与 `RenderDeviceExclusive::BeginViewport/EndViewport` 都是 stub。要打通「客户端窗口 / 编辑器小窗 / thumbnail 等多视口」→ RDG → 各后端 present 的完整链路，需要补全 `RenderViewport` 基类契约、落地 `ClientViewport`（swapchain-backed）、把 RDG 直接绑定 viewport、并让多 viewport 共享全局 inflight frame（保证共享 dynamic buffer 稳定）。

## What Changes

- **补全 `RenderViewport` 基类契约**：`Begin()`（check swapchain 状态 + 重建，失败取消当帧 present）/ `Acquire()`（拿 backbuffer）/ `Release()`（present）+ backbuffer/format/extent/name + acquire/render-done sema 访问器。
- **新增 `ClientViewport`**：继承 `RenderViewport`，持有 `SwapChain` + per-image acquire/render-done semaphore（按 `imageIndex` 索引），实现 Begin/Acquire/Release。
- **`SwapChain` 增加状态查询**：`SwapChainStatus { OK, OUT_OF_DATE, LOST }` + `GetStatus()`（后端自查，surface 尺寸由原生窗口决定，RHI 层不决定目标尺寸）；`Begin()` 依据状态尝试 `Resize` 重建。
- **RDG 直接绑定 viewport**：新增 `RenderGraph::BindViewport(name, viewport)`（`ViewportImageTag` 资源，culling 种子），prepare 阶段调用 `viewport->Acquire()` 解析 backbuffer（裸 `Image*`，无 refcount），替代 import backbuffer 的方式。
- **inflight frame 全局化**：`DeviceFrameContext` 持有全局 `mFrameIndex` + N 个 fence + 共享 dynamic buffer；多 viewport 一帧内共享同一 frame index，`SubmitInfo.fence` 用全局 fence（wait/signal 都走 frameContext）。
- **补齐 DX12 `D3D12SwapChain` present**（承接 `aurora-queue-submit-present` 任务 3.5）：`IDXGISwapChain3` + `GetBuffer` + `Present` + `Resize` + `GetStatus`。
- 修正并适配 `RenderDeviceExclusive::BeginViewport/EndViewport`。

## Capabilities

### New Capabilities

- `aurora-render-viewport`: `RenderViewport` 基类契约 + `ClientViewport`（swapchain-backed）+ `BindViewport`（RDG 绑定）+ `SwapChainStatus`（状态查询/重建）+ 全局 inflight frame 共享（`DeviceFrameContext`）。

### Modified Capabilities

- `aurora-swapchain`: `SwapChain` 增加 `SwapChainStatus GetStatus()`（状态自查）。
- `aurora-rdg`: `RenderGraph` 增加 `BindViewport` + `ViewportImageTag` 资源（prepare 阶段 acquire，culling 种子）。

## Impact

- **新文件**：`include/aurora/rdg/ClientViewport.h`、`src/rdg/ClientViewport.cpp`。
- **改动**：`include/aurora/rdg/RenderViewport.h`（扩展）、`src/rdg/RenderViewport.cpp`、`RenderDeviceExclusive.{h,cpp}`、`RenderGraph.{h,cpp}` + `RDGGraph.h`（`ViewportImageTag`）、`Compile.cpp`（prepare acquire）、`SwapChain.h` + 三后端 SwapChain（`GetStatus`）、DX12 `D3D12SwapChain`（补齐）。
- **测试**：`AuroraRHITest` 加 `ClientViewport` smoke test + RDG `BindViewport`/Present 结构断言。
- **不影响**：旧 render/core legacy、`OpaquePass`/`ScenePass`/`TextureToScreenPass` 现有行为。
