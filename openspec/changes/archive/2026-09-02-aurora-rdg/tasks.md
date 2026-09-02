## 1. 模块脚手架

- [x] 1.1 新建 `engine/aurora/rdg/` 目录与子目录（include/aurora/rdg/、src/、test/）
- [x] 1.2 写 `aurora/rdg/CMakeLists.txt`：`Aurora.RDG` STATIC 静态库，链 `Aurora.RHI`
- [x] 1.3 在 `engine/aurora/CMakeLists.txt` 加 `add_subdirectory(rdg)`（在 rhi 之后）
- [x] 1.4 删除老的空 stub `aurora/rhi/interface/include/aurora/rdg/RenderGraph.h` 与 `interface/src/rdg/RenderGraph.cpp`；调整 Aurora.RHI 的 GLOB 不再扫到这些
- [x] 1.5 在 `aurora/rdg/test/CMakeLists.txt` 加 `RDGTest`（gtest 风格，链 Aurora.RDG + AuroraVulkan + 可选 AuroraMetal）

## 2. 接口层

- [x] 2.1 `aurora/rdg/RDGHandles.h`：`RDGTextureHandle` / `RDGBufferHandle` 强类型 + `IsValid()` + `operator==/!=` + `std::hash` 特化
- [x] 2.2 `aurora/rdg/RDGTypes.h`：`RDGTextureDesc`（extent/format/usage/samples/mipLevels/residency/trackSubresource）、`RDGBufferDesc`（size/usage/residency）、`RDGAccessInfo`（AccessFlags + 可选 stage 覆盖）；`ResourceResidency` 枚举（TRANSIENT/PERSISTENT），v1 只实现 Transient；`trackSubresource` 预留（v2 mip/layer 精确跟踪）
- [x] 2.3 `aurora/rdg/RDGGraph.h`：内部图结构（hand-rolled，无 boost）
  - `ResourceNode` / `PassNode` / `AccessRecord` / `LifeTime` 结构
  - `ResourceTag` / `PassTag` tag-variant + 平行数组（`names[]/tags[]/payloadIndex[]` + 各类型 payload 数组）
  - payload 的 desc 携带 `residency`（Transient/Persistent，v1 只 Transient）
  - 访问链存于 `ResourceNode.accesses`（v1 用 per-resource 顺序链，等价于全局 arena 的连续切片）
- [x] 2.4 `aurora/rdg/RDGContext.h`：execute lambda 收的 ctx 类型；持 `cmdBuf*`、handle→Image/Buffer 映射、pass name
- [x] 2.5 `aurora/rdg/RenderGraphBuilder.h`：3 个 builder 类型 (`RasterPassBuilder` / `ComputePassBuilder` / `CopyPassBuilder`)，含 Read/Write/ColorAttachment/DepthStencilAttachment/Src/Dst
- [x] 2.6 `aurora/rdg/RenderGraph.h`：`RenderGraph` 主类含 Build/Import/Create/AddXxxPass/Compile/Execute/MarkOfInterest

## 3. Compile 实现

- [x] 3.1 `src/RenderGraph.cpp`：基础数据结构（ResourceNode / PassNode / AccessRecord）+ AddXxxPass 实现（即时调 setup callback 收集 builder 状态）
- [x] 3.2 `src/RenderGraph.cpp`：`AddDependency(res, pass, access)` 访问链构建
  - 链尾同 pass + 同 access（连续 read）就地合并；否则追加 `AccessRecord`
  - 同 pass 内多 access 做 OR 合并
- [x] 3.3 `src/Compile.cpp`：由访问链推导 pass 依赖边（每个资源：writer pass → 后续 reader pass），填 `PassNode.dependsOn`
- [x] 3.4 `src/Compile.cpp`：Kahn 拓扑排序（手写，用 `dependsOn` 算入度）
- [x] 3.5 `src/Compile.cpp`：生命周期 (firstUsePass, lastUsePass) 从每条访问链首尾计算
- [x] 3.6 `src/Compile.cpp`：pass culling 反向 BFS；live 集合
- [x] 3.7 `src/Compile.cpp`：barrier 推导（v1 整资源）
  - 顺序扫访问链，相邻两条 access 不同时发整资源 barrier
  - 调 `aurora/rhi/Barrier.h` 的 `InferLayoutForAccess` 填 oldLayout/newLayout；srcStage/dstStage 由 access 查表
  - 第一次使用前从 UNDEFINED；export 资源最后转目标 layout（如 PRESENT）
  - 聚合进 pass 的 frontBarriers / rearBarriers
- [x] 3.8 `src/TransientPool.cpp`：`TransientPool` 抽象接口 + 对象池实现（堆池预留）
  - 抽象接口：`AcquireImage(desc)` / `AcquireBuffer(desc)` / `ReleaseImage` / `ReleaseBuffer`
  - 对象池：AllocKey = 完整 desc `(format, width, height, depth, mipLevels, arrayLayers, samples, usage)`；lifetime 不重叠复用；跨帧 LRU
  - 堆池：v2 预留（memory heap 级 aliasing，只留接口不实现）
  - Compile 期对 transient handle 调 Acquire；Execute 结束后归还
- [x] 3.9 `src/Compile.cpp`：把每个 transient handle 绑定到具体 RHI Image / Buffer

## 4. Execute 实现

- [x] 4.1 `src/Execute.cpp`：按拓扑序遍历 live pass（Kahn 排序结果）
- [x] 4.2 每 pass 之前 emit pre-barrier `cmdBuf->PipelineBarrier(...)`
- [x] 4.3 按 pass 类型创建 encoder：
  - Raster：CreateGraphicsEncoder + BeginRendering（用 setup 期收集的 ColorAttachment / DepthStencilAttachment 构造 RenderingInfo）
  - Compute：CreateComputeEncoder
  - Copy：CreateBlitEncoder
- [x] 4.4 调用 user execute lambda(encoder, ctx)
- [x] 4.5 Raster pass 的 EndRendering；encoder 销毁
- [x] 4.6 Final barriers：export 资源转目标 layout（如 PRESENT）

## 5. 测试

- [x] 5.1 `RDGTest::SinglePassClear`（Vulkan）：单 raster pass 清屏 import 进来的 image，Execute + submit + fence 验证（readback 像素级验证与 5.2/5.3/5.7 一并延后）
- [ ] 5.2 `RDGTest::TwoPassChain`：pass A 写 transient → pass B 读 transient → readback 验证；validation 不报警（延后：需 shader/pipeline 采样 infra）
- [ ] 5.3 `RDGTest::ComputeToGraphics`：compute pass 写 storage buffer → raster pass 读 → barrier 自动推导验证（延后：需 compute shader infra）
- [x] 5.4 `RDGTest::PassCulling`：pass A 写 transient T 但无消费者；Compile 后 A 在 livePasses 之外
- [x] 5.5 `RDGTest::TransientAliasing`：同帧 4 个 1080p RGBA8 lifetime 不重叠 → pool 分配 1 个底层 image，命中 3 次
- [x] 5.6 `RDGTest::TransientCrossFrame`：连续 5 帧相同 graph → 第 2 帧起 hit 率 100%
- [ ] 5.7 `RDGTest::ImportSwapChainImage`：import swapchain image → raster pass 写 → 自动 final transition 到 PRESENT；Vulkan validation 不报 layout 错（延后：需 swapchain/window infra）
- [x] 5.8 `RDGTest::HandleTypeSafety`（compile-only test 或 SFINAE 检查）：textureHandle 传给 Buffer 接口编译失败
- [x] 5.9 `RDGTest::HandleEqualityHash`：handle 作为 unordered_map key 工作正常

## 6. 调试 / 文档

- [x] 6.1 RDG debug logger：Compile 完成时输出 live/culled passes、transient pool hit/miss
- [x] 6.2 `engine/aurora/AGENTS.md` 加一节"RDG 用法"：三段式 lifecycle、handle 强类型、execute lambda 不得手写 barrier
- [x] 6.3 在 RDG header 顶部写"何时不该用 RDG"：低复杂度场景（demo、tool）直接用 Encoder + 手写 PipelineBarrier 仍是合法路径

## 7. 收尾

- [x] 7.1 跑 `cmake --build`（本机 Windows/Vulkan 通过；macOS/Metal 未在本环境验证，代码平台无关）
- [x] 7.2 跑 `RDGTest`（Vulkan 全绿：SinglePassClear / PassCulling / TransientAliasing / TransientCrossFrame / RDGHandleTest；Metal 未在本环境验证）
- [x] 7.3 Vulkan validation layer（debug layer 开启）无新 warning；Metal validation 未在本环境验证
- [ ] 7.4 archive：`openspec archive aurora-rdg`
