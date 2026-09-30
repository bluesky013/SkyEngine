## Why

Aurora 已有 scene / pipeline / RDG，但**没有渲染主循环**：渲染逻辑寄生在 adaptor 的
`AuroraModule::Tick`，在主线程直接 clear / submit / present；`core/Renderer.h` 只是 TODO stub，
`RenderDeviceExclusive` 空转。要支撑后续真实场景渲染与编辑器多 viewport，需要一个独立 render
线程，以及一条主线程到 `RenderScene` 的线程安全同步 seam。

## What Changes

- 新增 `engine/aurora/renderer`（STATIC）：`Renderer` 入口、专用 render 线程（非 `NamedThread`）、
  SPSC `SceneCommand` mailbox、per-scene `ScenePipeline`、`SceneRenderContext`、`RenderSceneId` /
  `ViewportId`（主线程分配，index+generation）。
- `Instance` / `Device` 完全 confine 到 render 线程；主线程经命令 mailbox 提交场景/视口意图
  （fire-and-forget），不再直接持有 Device。
- `AuroraModule` 瘦身为转发（`Init`/`Start`/`Tick`/`Shutdown` → `Renderer`），移除内联
  Device / CommandPool / CommandBuffer / ClientViewport 逻辑。
- `PipelinePass::BuildRDG(graph, ctx)` 参数化：scene 由 pipeline 持有，`view` / `extent` / `output`
  作为每 viewport 上下文传入。
- 新增 `RenderSceneProxy`（adaptor，`IWorldSubSystem`）作为主线程场景句柄 → 命令入队。
- editor 多 viewport：`RenderViewportProxy`，运行期 create / destroy / resize；一帧单 cmd buffer、
  单 Submit、全局 fence；`Begin`/`Acquire` 失败的 viewport 取消当帧 present 但不影响其它。
- **BREAKING**：`PipelinePass` / `SceneRasterPassTemplate` 的 `BuildRDG` 签名与 scene/view 归属改变；
  移除 `core/Renderer.h` stub；废弃 `RenderDeviceExclusive`。

## Capabilities

### New Capabilities

- `aurora-renderer`: render 线程 + 主循环 + 命令 mailbox + per-scene ScenePipeline + 多 viewport
  帧驱动 + 主线程 `RenderSceneProxy` seam。

### Modified Capabilities

- `aurora-pipeline`: `BuildRDG(graph, ctx)` 参数化；pass 的 scene 归属与 view/extent/output 上下文契约。
- `aurora-adaptor`: `AuroraModule` 改为转发 `Renderer`，移除内联渲染循环。

## Non-goals

- shader 资产 / 编译缓存 / 变体 key（见 change `aurora-shader-cache`）。
- material → PSO / ResourceGroup / DrawItem / 分桶（见 change `aurora-material-pso`）。
- Metal swapchain 线程约束（后续 spike，本 change 先保 Vulkan / DX12）。

## Impact

- 新增 `engine/aurora/renderer`；改 `engine/aurora/adaptor`（`AuroraModule`、`RenderSceneProxy`）、
  `engine/aurora/pipeline`（pass 签名）、`engine/aurora/core`（删除 Renderer stub）、
  `engine/aurora/rhi/interface`（废弃 `RenderDeviceExclusive`）。
- `engine/aurora/CMakeLists.txt` 增加 `add_subdirectory(renderer)`；`adaptor` link `Aurora.Renderer`。

## Open Questions

- 帧节拍：主线程驱动（每 Tick 唤醒一帧）vs render 线程自跑 + 事件驱动。
- per-scene `ScenePipeline` 与 `TechniqueCache`（C3）的边界划分。
