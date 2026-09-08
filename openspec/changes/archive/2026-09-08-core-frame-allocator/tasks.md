## 1. core 层：LinearStorage 检查点

- [x] 1.1 `LinearStorage::Mark` 结构体（`size_t blockIndex; size_t offset`）+ `GetMark()` 返回当前 block 索引与 offset
- [x] 1.2 `LinearStorage::Rewind(Mark)` — 丢弃 mark 之后的 block，回退 mark 所在 block 的 offset；`mCurrentBytes` 相应回退
- [x] 1.3 `LinearStorage` 测试：`Mark/Rewind` 基本、跨 block 回退、回退后复用、边界条件（mark 在 block 边界）

## 2. core 层：TransientAllocator 透传与别名

- [x] 2.1 `TransientAllocator` 透传 `GetMark()` / `Rewind(Mark)` / `Arena()`（返回自身引用）
- [x] 2.2 `TransientAllocator` 新增 `GetMark()` / `Rewind(Mark)` 委托 `LinearStorage`
- [x] 2.3 `TransientAllocator` 测试：`Mark/Rewind` 透传正确性

## 3. core 层：TransientVector 补全

- [x] 3.1 `TransientAllocator.h` 补 `TransientVector<T>` 别名（`std::vector<T, TransientStdAllocator<T>>`）
- [x] 3.2 `TransientAllocator.h` 补 `MakeTransientVector<T>(TransientAllocator&)` 工厂
- [x] 3.3 `TransientAllocatorTest.cpp` 补 `TransientVector` 基本/多类型/跨帧测试

## 4. core 层：FrameAllocator 门面

- [x] 4.1 新增 `core/memory/FrameAllocator.h`：`Allocate` / `AllocateArray` / `Construct` / `GetMark` / `Rewind` / `Arena` / `Reset` / `GetCurrentBytes` / `GetPeakBytes` / `GetAllocationCount`
- [x] 4.2 `FrameAllocator` 内部：`LinearStorage mStorage` + `TransientAllocator mArena`（external-storage 构造）
- [x] 4.3 统计字段：`size_t mCurrentBytes = 0; size_t mPeakBytes = 0; size_t mAllocationCount = 0;`（`Allocate`/`Rewind`/`Reset` 内原子更新）
- [x] 4.4 `FrameAllocator` 测试：基本分配、检查点/回退、统计正确性、`Construct`、跨 block、与 `TransientVector` 配合

## 5. aurora RDG：setup graph 迁移

- [x] 5.1 `RenderGraph.h`：`Build(Device*, FrameAllocator&)`；`mResources`/`mPasses`/`mImages`/`mImportImages`/`mBuffers`/`mImportBuffers`/`mTopologicalOrder`/`mLivePasses` 改为 `TransientVector<T>`；`mResolvedImages`/`mResolvedBuffers` 改为 `TransientVector<Image*>`/`TransientVector<Buffer*>`
- [x] 5.2 `RenderGraph.cpp`：所有 `emplace_back`/`push_back` 走 `TransientVector`；`AddResource`/`AddPass`/`CreateTexture`/`CreateBuffer`/`Import` 签名不变（Name + ImagePtr/BufferPtr 已迁移）
- [x] 5.3 `RDGGraph.h`：`GraphImage`/`GraphImportImage`/`GraphBuffer`/`GraphImportBuffer`/`AccessRecord`/`PassNode`/`ResourceNode` 等结构内 `std::vector` 字段改为 `TransientVector`（或保持 POD，视结构复杂度）

## 6. aurora RDG：CompiledGraph 产出

- [x] 6.1 新增 `CompiledGraph` 结构（`aurora/rdg/CompiledGraph.h`）：`TransientVector<CompiledPass> passes`；`TransientVector<BarrierInfo> barriers`；`TransientVector<Image*> resolvedImages`；`TransientVector<Buffer*> resolvedBuffers`；`TransientVector<uint32_t> topologicalOrder`
- [x] 6.2 `CompiledPass` 结构：`PassType type`；`uint32_t passIndex`；`uint32_t barrierOffset`/`barrierCount`（指向全局 barriers 数组的连续段）；`TransientVector<RasterAttach> colors`/`TransientVector<DepthStencilAttach> depthStencil`；`TransientVector<RenderItem> items`（v1 可先空）
- [x] 6.3 `RenderGraph::Compile()` 产出 `CompiledGraph` 到同一 `FrameAllocator`；setup graph 编译后 `Rewind` 回收（或保留 setup 供 debug，编译产物独立）

## 7. aurora RDG：DeviceFrameContext 持有 FrameAllocator

- [x] 7.1 `RenderDeviceExclusive.h`：`DeviceFrameContext` 新增 `FrameAllocator mFrameAllocator`；`GetFrameAllocator()` 返回引用
- [x] 7.2 `RenderDeviceExclusive.cpp`：`BeginFrame()` 初始化/检查 allocator；`EndFrame()` 调 `mFrameAllocator.Reset()`
- [x] 7.3 `RenderGraph::Build` 从 `RenderDeviceExclusive::Get().GetDevice()->GetFrameContext()` 或调用方传入的 `FrameAllocator` 取 arena

## 8. 测试与验证

- [x] 8.1 `engine/test/core/LinearStorageTest.cpp`：补 `Mark/Rewind` 测试
- [x] 8.2 `engine/test/core/TransientAllocatorTest.cpp`：补 `Mark/Rewind`/`TransientVector` 测试
- [x] 8.3 `engine/test/core/FrameAllocatorTest.cpp`（新增）：基本/检查点/统计/Construct/跨 block
- [x] 8.4 `engine/aurora/rhi/test/RDGTest.cpp`：`RenderGraph::Build(device, frameAlloc)` 适配；断言 `FrameAllocator` 统计非零
- [x] 8.5 全量 `cmake --build` 通过；`AuroraTest --gtest_filter=RDG*` 全绿；`CoreTest` 全绿

## 9. 收尾

- [ ] 9.1 `openspec archive core-frame-allocator` 归档本 change
- [ ] 9.2 后续 RDG 数据驱动重构 change 依赖本 change（`RenderGraph` 已接收 `FrameAllocator&`）
