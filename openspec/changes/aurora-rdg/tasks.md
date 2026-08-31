## 1. 模块脚手架

- [ ] 1.1 新建 `engine/aurora/rdg/` 目录与子目录（include/aurora/rdg/、src/、test/）
- [ ] 1.2 写 `aurora/rdg/CMakeLists.txt`：`Aurora.RDG` STATIC 静态库，链 `Aurora.RHI`
- [ ] 1.3 在 `engine/aurora/CMakeLists.txt` 加 `add_subdirectory(rdg)`（在 rhi 之后）
- [ ] 1.4 删除老的空 stub `aurora/rhi/interface/include/aurora/rdg/RenderGraph.h` 与 `interface/src/rdg/RenderGraph.cpp`；调整 Aurora.RHI 的 GLOB 不再扫到这些
- [ ] 1.5 在 `aurora/rdg/test/CMakeLists.txt` 加 `RDGTest`（gtest 风格，链 Aurora.RDG + AuroraVulkan + 可选 AuroraMetal）

## 2. 接口层

- [ ] 2.1 `aurora/rdg/RDGHandles.h`：`RDGTextureHandle` / `RDGBufferHandle` 强类型 + `IsValid()` + `operator==/!=` + `std::hash` 特化
- [ ] 2.2 `aurora/rdg/RDGTypes.h`：`RDGTextureDesc`（extent/format/usage/samples/mipLevels）、`RDGBufferDesc`（size/usage）、`RDGAccessInfo`（AccessFlags + 可选 stage 覆盖）
- [ ] 2.3 `aurora/rdg/RDGContext.h`：execute lambda 收的 ctx 类型；持 `cmdBuf*`、handle→Image/Buffer 映射、pass name
- [ ] 2.4 `aurora/rdg/RenderGraphBuilder.h`：3 个 builder 类型 (`RasterPassBuilder` / `ComputePassBuilder` / `CopyPassBuilder`)，含 Read/Write/ColorAttachment/DepthStencilAttachment/Src/Dst
- [ ] 2.5 `aurora/rdg/RenderGraph.h`：`RenderGraph` 主类含 Build/Import/Create/AddXxxPass/Compile/Execute/MarkOfInterest

## 2b. FrameGraphDispatcher（Compile 并行调度）

- [ ] 2b.1 `core/async/ThreadPool` 加 public `Schedule(ThreadTask&&)`（round-robin 入队）
- [ ] 2b.2 `aurora/rhi/interface/include/aurora/rdg/FrameGraphDispatcher.h`：单线程构建 / 批次提交 / index 引用
- [ ] 2b.3 `aurora/rhi/interface/src/rdg/FrameGraphDispatcher.cpp`：无锁无引用计数实现
- [ ] 2b.4 `aurora/rhi/test/FrameGraphDispatcherTest.cpp`：线性链 / diamond / 多根 / 空批次 / Clear 复用 / future
- [ ] 2b.5 `aurora/rhi/test/FrameGraphDispatcherBenchmark.cpp`：TaskNode vs FrameGraphDispatcher 对比
- [ ] 2b.6 spec `aurora-rdg-dispatcher`

## 3. Compile 实现

- [ ] 3.1 `src/RenderGraph.cpp`：基础数据结构（Pass / Resource / UseRecord）+ AddXxxPass 实现（即时调 setup callback 收集 builder 状态）
- [ ] 3.2 `src/Compile.cpp`：拓扑排序 pass（按 read/write 依赖）
- [ ] 3.3 `src/Compile.cpp`：每个资源的使用序列 + (firstUsePass, lastUsePass) 计算
- [ ] 3.4 `src/Compile.cpp`：pass culling 反向 BFS；live 集合
- [ ] 3.5 `src/Compile.cpp`：barrier 推导
  - 调 `aurora-encoder-barriers` 的 `InferLayoutForAccess`
  - 相邻 use 之间生成 BarrierInfo（含 srcAccess/dstAccess/oldLayout/newLayout/srcStage/dstStage）
  - 同 access 连续 read 合并不发 barrier
  - 第一次使用前从 UNDEFINED；export 资源最后转目标 layout
- [ ] 3.6 `src/TransientPool.cpp`：device-级 alias-aware allocator
  - 按 (extent, format, samples, usage) hash 分桶
  - lifetime 不重叠时复用
  - Compile 期对 transient handle 调 Acquire；Execute 结束后归还
  - 跨帧 LRU 淘汰（默认 N=3 帧未用即删）
- [ ] 3.7 `src/Compile.cpp`：把每个 transient handle 绑定到具体 RHI Image / Buffer

## 4. Execute 实现

- [ ] 4.1 `src/Execute.cpp`：遍历 live pass
- [ ] 4.2 每 pass 之前 emit pre-barrier `cmdBuf->PipelineBarrier(...)`
- [ ] 4.3 按 pass 类型创建 encoder：
  - Raster：CreateGraphicsEncoder + BeginRendering（用 setup 期收集的 ColorAttachment / DepthStencilAttachment 构造 RenderingInfo）
  - Compute：CreateComputeEncoder
  - Copy：CreateBlitEncoder
- [ ] 4.4 调用 user execute lambda(encoder, ctx)
- [ ] 4.5 Raster pass 的 EndRendering；encoder 销毁
- [ ] 4.6 Final barriers：export 资源转目标 layout（如 PRESENT）

## 5. 测试

- [ ] 5.1 `RDGTest::SinglePassClear`（Vulkan）：单 raster pass 清屏 import 进来的 buffer/image，readback 像素正确
- [ ] 5.2 `RDGTest::TwoPassChain`：pass A 写 transient → pass B 读 transient → readback 验证；validation 不报警
- [ ] 5.3 `RDGTest::ComputeToGraphics`：compute pass 写 storage buffer → raster pass 读 → barrier 自动推导验证
- [ ] 5.4 `RDGTest::PassCulling`：pass A 写 transient T 但无消费者；Compile 后 A 在 livePasses 之外
- [ ] 5.5 `RDGTest::TransientAliasing`：同帧 4 个 1080p RGBA8 lifetime 不重叠 → pool 分配 1 个底层 image，命中 3 次
- [ ] 5.6 `RDGTest::TransientCrossFrame`：连续 5 帧相同 graph → 第 2 帧起 hit 率 100%
- [ ] 5.7 `RDGTest::ImportSwapChainImage`：import swapchain image → raster pass 写 → 自动 final transition 到 PRESENT；Vulkan validation 不报 layout 错
- [ ] 5.8 `RDGTest::HandleTypeSafety`（compile-only test 或 SFINAE 检查）：textureHandle 传给 Buffer 接口编译失败
- [ ] 5.9 `RDGTest::HandleEqualityHash`：handle 作为 unordered_map key 工作正常

## 6. 调试 / 文档

- [ ] 6.1 RDG debug logger：Compile 完成时输出 live/culled passes、transient pool hit/miss
- [ ] 6.2 `engine/aurora/AGENTS.md` 加一节"RDG 用法"：三段式 lifecycle、handle 强类型、execute lambda 不得手写 barrier
- [ ] 6.3 在 RDG header 顶部写"何时不该用 RDG"：低复杂度场景（demo、tool）直接用 Encoder + 手写 PipelineBarrier 仍是合法路径

## 7. 收尾

- [ ] 7.1 跑 `cmake --build` 在 macOS（Vulkan + Metal）通过
- [ ] 7.2 跑 `RDGTest` 全套绿（Vulkan 必跑、Metal 选跑）
- [ ] 7.3 Vulkan validation layer / Metal validation 不报新 warning
- [ ] 7.4 archive：`openspec archive aurora-rdg`
