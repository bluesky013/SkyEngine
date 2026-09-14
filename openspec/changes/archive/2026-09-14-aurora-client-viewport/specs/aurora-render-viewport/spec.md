## ADDED Requirements

### Requirement: RenderViewport 三阶段生命周期契约

`RenderViewport` SHALL 定义三阶段生命周期：`Begin()`（check swapchain 状态 + 重建，false = 取消当帧 present）、`Acquire()`（拿下一帧 backbuffer，false = 本帧不可渲染）、`Release()`（present，取消帧为 no-op）。

`RenderViewport` SHALL 暴露：`GetBackbuffer()`（Acquire 成功后有效）、`GetFormat()`、`GetExtent()`、`GetName()`，以及供 Submit 用的 `GetAcquireSemaphore()` / `GetRenderDoneSemaphore()`（当前帧 ring 的 BINARY sema）。基类 SHALL NOT 暴露 frame index 或 fence（全局化，见「全局 inflight frame」需求）。

#### Scenario: Begin 失败取消 present

- **WHEN** `viewport->Begin()` 返回 false（swapchain 状态异常且重建失败）
- **THEN** 帧驱动方跳过本帧（不建 RDG、不 Submit、不 Present）

#### Scenario: Acquire 后 backbuffer 有效

- **WHEN** `viewport->Begin()` 成功且 `viewport->Acquire()` 返回 true
- **THEN** `GetBackbuffer()` 返回非空 `Image*`（持有 RENDER_TARGET），`GetFormat()`/`GetExtent()` 与表面一致

### Requirement: ClientViewport 持有 SwapChain 与 per-image sema

`ClientViewport` SHALL 继承 `RenderViewport`，`Init(Device*, const SwapChain::Descriptor&)` 创建 SwapChain 并分配 `imageCount` 组 acquire/render-done BINARY semaphore（按 per-viewport 帧 ring 索引，本地 acquire 计数器）。

`Begin()` SHALL 依据 `GetStatus()` 重建；`Acquire()` SHALL 调 `AcquireNextImage(acquireSema[slot], nullptr, UINT64_MAX)` 并记 image index；`Release()` SHALL 在帧有效时调 `Present(imageIndex, 1, &renderDoneSema[slot])`。`ClientViewport` SHALL NOT 持有 frame index / fence（全局化）。

#### Scenario: Acquire 返回有效索引

- **WHEN** `ClientViewport::Init` 成功后 `Begin()` + `Acquire()`
- **THEN** `Acquire()` 返回 true；backbuffer 非空；acquire sema / render-done sema 均非空

#### Scenario: 取消帧的 Release 为 no-op

- **WHEN** `Begin()` 或 `Acquire()` 失败后调用 `Release()`
- **THEN** 不调用 `SwapChain::Present`，不崩溃

### Requirement: SwapChainStatus 自查与重建

`ClientViewport::Begin()` SHALL 查询 `SwapChain::GetStatus()`：`OK` → 直接继续；`OUT_OF_DATE` → 调 `Resize` 重建；`LOST` 或 `Resize` 失败 → 取消当帧 present。surface 尺寸由原生窗口决定，RHI 层不决定目标尺寸；`SUBOPTIMAL` SHALL 当作 `OK` 处理。

#### Scenario: OUT_OF_DATE 触发重建

- **WHEN** 窗口尺寸变化导致 `GetStatus()` 返回 `OUT_OF_DATE`
- **THEN** `Begin()` 触发 `Resize`；重建成功后本帧正常渲染；`GetExtent()` 反映新尺寸

#### Scenario: 重建失败取消 present

- **WHEN** `GetStatus()` 返回 `LOST` 或 `Resize` 返回失败
- **THEN** `Begin()` 返回 false；本帧 present 被取消，不崩溃

### Requirement: 全局 inflight frame 共享

`DeviceFrameContext` SHALL 持有全局 `mFrameIndex` + `mInflightNum` 个 fence + `FrameAllocator`。`BeginFrame()` SHALL `Wait` 并 `Reset` `fence[mFrameIndex % N]` 后自增 `mFrameIndex`；共享 dynamic buffer（`BatchAllocator`/`GlobalRenderResources`/`TransientBufferPool`）SHALL 按 `mFrameIndex % N` 分配。

同一帧内多个 viewport SHALL 共享同一 `mFrameIndex`；各 viewport 的 `SubmitInfo.fence` SHALL 使用全局 `fence[mFrameIndex % N]`。`GetFrameFence()` SHALL 返回当前帧 fence。

#### Scenario: 多 viewport 同帧共享 dynamic buffer 稳定

- **WHEN** 一帧内主窗 + 编辑器小窗两个 viewport 各自 Acquire/Render/Release，共享的 dynamic buffer 按同一 `mFrameIndex` 分配
- **THEN** 两 viewport 读写同一帧的 dynamic buffer 不互相覆盖（fence 保证 N 帧前的 GPU 工作完成）

### Requirement: 各后端 present 映射

`ClientViewport` SHALL 只依赖 `SwapChain`/`Queue`/`Semaphore`/`Fence` 抽象，后端无关。present 差异封闭在各后端 `SwapChain` 实现：

- Vulkan：acquire `vkAcquireNextImageKHR` signal binary；render-done submit signal binary；present `vkQueuePresentKHR` wait。
- DX12：acquire `GetCurrentBackBufferIndex`（acquire sema 立即 signal）；render-done `ID3D12Fence::Signal`；present `IDXGISwapChain3::Present`。
- Metal：acquire `nextDrawable` + 立即 signal；render-done `MTLEvent` signal；present `presentDrawable:` 前 `encodeWaitForEvent`。

#### Scenario: 三后端接口一致

- **WHEN** 同一 `ClientViewport` 调用序列跑在 Vulkan / DX12 / Metal 任一后端
- **THEN** `Begin`/`Acquire`/`Release` 接口与顺序一致，后端差异不泄漏到调用方
