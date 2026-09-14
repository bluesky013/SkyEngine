## 1. SwapChain 状态自查

- [x] 1.1 `aurora/rhi/SwapChain.h`：加 `enum class SwapChainStatus { OK, OUT_OF_DATE, LOST }` + `virtual SwapChainStatus GetStatus() const = 0`
- [x] 1.2 `VulkanSwapChain::GetStatus`：surface caps extent 与自身 extent 比对 + 反映 `vkAcquireNextImageKHR` 的 `OUT_OF_DATE`/`SURFACE_LOST`（`SUBOPTIMAL` 当 `OK`）
- [x] 1.3 `MetalSwapChain::GetStatus`：`CAMetalLayer` 尺寸 vs 自身 extent
- [x] 1.4 `D3D12SwapChain::GetStatus`（随 §6 一起）

## 2. RenderViewport 基类契约

- [x] 2.1 重写 `include/aurora/rdg/RenderViewport.h`：三阶段 `Begin()`/`Acquire()`/`Release()` + `GetBackbuffer/GetFormat/GetExtent/GetName` + `GetAcquireSemaphore/GetRenderDoneSemaphore`（不含 frame index/fence）
- [x] 2.2 更新 `src/rdg/RenderViewport.cpp`（删除 `ImportBackbuffer`，如有残留清理）

## 3. ClientViewport 具体实现

- [x] 3.1 `include/aurora/rdg/ClientViewport.h`：继承 `RenderViewport`；`Init(Device*, const SwapChain::Descriptor&)`；成员 `SwapChainPtr` + `mAcquireSemas/mRenderDoneSemas`（imageCount）+ `mImageIndex` + `mFrameValid`
- [x] 3.2 `src/rdg/ClientViewport.cpp`：`Init` 创建 SwapChain + 分配 imageCount 组 BINARY sema
- [x] 3.3 `Begin()`：`GetStatus()` → `OUT_OF_DATE` 则 `Resize`；`LOST`/`Resize` 失败 → `mFrameValid=false`、返回 false
- [x] 3.4 `Acquire()`：`AcquireNextImage(mAcquireSemas[imageIndex], nullptr, UINT64_MAX)`；`INVALID_INDEX` → `mFrameValid=false`、返回 false
- [x] 3.5 `Release()`：`mFrameValid` 时 `Present(mImageIndex, 1, &mRenderDoneSemas[imageIndex])`

## 4. DeviceFrameContext 全局 inflight frame

- [x] 4.1 `RenderDeviceExclusive.h` 的 `DeviceFrameContext`：加 `std::vector<FencePtr> mFences`（N）+ `GetFrameFence()`
- [x] 4.2 `RenderDeviceExclusive.cpp`：`BeginFrame()` 里 `Wait` + `Reset` `fence[F % N]` 后 `++mFrameIndex`；`EndFrame()` 保持 reset allocator
- [ ] 4.3 共享 dynamic buffer（`BatchAllocator`/`GlobalRenderResources`/`TransientBufferPool`）按 `mFrameIndex % N` 分配（若尚未按此索引，补上）

## 5. RDG BindViewport

- [x] 5.1 `RDGGraph.h`：`ResourceTag` 加 `ViewportImageTag`（存 `RenderViewport*`）
- [x] 5.2 `RenderGraph.h/.cpp`：加 `BindViewport(const Name&, RenderViewport*) -> RDGTextureHandle`
- [x] 5.3 `Compile.cpp` `CullPasses`：`ViewportImageTag` 作为 culling 种子
- [x] 5.4 `Compile.cpp` `BindTransientResources`：viewport 资源调 `Acquire()` → 成功 `mResolvedImages[i] = GetBackbuffer()`；失败标记 unresolved（写者 pass cull）

## 6. DX12 SwapChain present 补齐

- [x] 6.1 `D3D12SwapChain.h/.cpp`：`IDXGISwapChain3` + `GetCurrentBackBufferIndex`（AcquireNextImage）+ `GetBuffer` 包装 `D3D12Image` + `Present`（`IDXGISwapChain3::Present`）+ `Resize`（`ResizeBuffers`）+ `GetStatus`
- [x] 6.2 DX12 acquire sema 立即 signal（无原生 acquire 信号），与 BINARY sema 语义对齐

## 7. RenderDeviceExclusive 适配

- [x] 7.1 修正 `RenderDeviceExclusive::BeginViewport` 的 `SKY_ASSERT(mCurrentViewport != nullptr)`（应为 `== nullptr`）
- [x] 7.2 `BeginViewport/EndViewport` 基于新 `RenderViewport` 契约记录/清空当前 surface

## 8. 测试

- [x] 8.1 `AuroraRHITest` `ClientViewport` smoke test：`Init` + `Begin`/`Acquire` 成功、`GetBackbuffer` 非空、acquire/render-done sema 非空
- [x] 8.2 RDG `BindViewport` 集成测试：`BindViewport` + 写 pass + `AddPresentPass` → Compile 后 backbuffer 存在 `PRESENT` access 末级 barrier、writer pass 不被 cull；`Acquire` 失败时 writer pass 被 cull
- [x] 8.3 `DeviceFrameContext` 测试：`BeginFrame` 递增 index、fence 复用；多 viewport 共享同一 `mFrameIndex`
- [x] 8.4 `RenderDeviceExclusive::BeginViewport/EndViewport`：连续 Begin/End 不触发 assert

## 9. 验证与收尾

- [x] 9.1 全量 `cmake --build` 通过
- [x] 9.2 `AuroraRHITest` 全绿（Vulkan + 当前 host 第二后端）
- [ ] 9.3 `openspec archive aurora-client-viewport` 归档本 change
