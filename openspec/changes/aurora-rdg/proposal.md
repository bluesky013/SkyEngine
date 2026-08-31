## Why

Aurora RHI 完成 Queue/Submit/Sync/SwapChain（aurora-queue-submit-present）+ Barrier（aurora-encoder-barriers）+ 资源绑定（aurora-resource-group）后，仍缺一个**自动管理资源生命周期、自动推导 barrier、自动调度 pass** 的上层。任何严肃渲染（PBR forward、deferred、TAA、阴影、bloom 等等）都需要 RDG 来：

- 让一帧的 pass 之间的资源依赖**显式化**，编译期就能发现"读未写"、"写后未读"的错误
- 把 barrier 从手写降成"声明读写 + RDG 推导"，调用方不再写 access mask / layout / pipeline stage
- 把 transient（per-frame）资源池化复用，省 VRAM
- 提供 pass culling（无下游消费的 pass 自动剔除）

当前 `aurora/rhi/interface/include/aurora/rdg/RenderGraph.h` 与 `RenderGraph.cpp` 是空 stub。本 change 把 RDG 真正立起来，作为 `aurora/core/Renderer` 主循环之下的渲染调度层。

## What Changes

- 在 **`Aurora.RDG`**（新建静态库，独立于 Aurora.RHI）中实现 RDG 核心
- 三段式 API：**Setup**（pass 声明读写）→ **Compile**（依赖图分析、资源生命周期、barrier 推导、transient 池分配）→ **Execute**（按拓扑顺序 emit barrier + encoder + 调用 pass body）
- **Resource handle**：`RDGBufferHandle` / `RDGTextureHandle` 是 opaque ID，pass 通过 handle 声明读写而不持有 RHI 资源
- **Resource declaration**：
  - `Import(rhi::Image*)` / `Import(rhi::Buffer*)`：导入外部资源（如 SwapChain image）
  - `CreateTexture(name, desc)` / `CreateBuffer(name, desc)`：transient 资源，由 RDG 池化分配
- **Pass declaration**：
  - `AddRasterPass(name, setup_lambda, execute_lambda)` — 图形 pass
  - `AddComputePass(name, setup_lambda, execute_lambda)` — 计算 pass
  - `AddCopyPass(name, setup_lambda, execute_lambda)` — 传输 pass
  - setup 阶段：`builder.Read(handle, access)` / `builder.Write(handle, access)` / `builder.ColorAttachment(slot, handle, loadOp, storeOp)` / `builder.DepthStencilAttachment(handle, ...)`
- **Compile 并行调度**：`FrameGraphDispatcher`（`aurora-rdg-dispatcher`）—— 单线程构建、批次提交、无锁无引用计数的依赖图调度器，通过 `ThreadPool::Schedule` 并行执行 Compile 期的拓扑排序 / barrier 推导 / 生命周期分析等 CPU 密集子任务
- **Compile**：
  - 拓扑排序 pass
  - 计算每个资源的"first-use → last-use"区间
  - 对 transient 资源走 alias-aware 池分配（生命周期不重叠的资源共享物理底座）
  - 在每条 pass 边上推导 barrier（src/dst access、layout transition、stage mask）— 调用 `aurora-encoder-barriers` 提供的 `InferLayoutForAccess` 工具
  - Pass culling：从"导出资源"（SwapChain image / 用户标记 of-interest）反向追溯，未被引用的 pass 剔除
- **Execute**：
  - 顺序遍历活 pass
  - 在 pass 之前调 `cmdBuf->PipelineBarrier(...)`（用上 change 的 cmdbuf-level API）
  - 创建对应类型 Encoder（GraphicsEncoder + BeginRendering / ComputeEncoder / BlitEncoder）
  - 调用用户 execute_lambda(encoder, ctx)
  - End encoder
- **Transient pool**：内部小型 alias-aware allocator，按 (extent, format, usage, sampleCount) hash 取 cached image；未命中创建新；按帧 LRU 淘汰
- **集成**：本 change 不直接接入 `aurora/core/Renderer`（那是后续 change），但提供 `RDG::Build(device) → unique_ptr<RenderGraph>` 让外部使用

## Capabilities

### New Capabilities
- `aurora-rdg`: RenderGraph 构建、编译、执行、transient 资源池化、自动 barrier 推导
- `aurora-rdg-handles`: Resource handle 抽象与生命周期分析
- `aurora-rdg-dispatcher`: Compile 期并行依赖图调度器（单线程构建 + 批次提交 + 无锁无引用计数）

### Modified Capabilities
（无既有 spec 修改）

## Impact

- **新模块**：`engine/aurora/rdg/` 独立目录（比 RHI interface 高一层），Aurora.RDG 静态库依赖 Aurora.RHI
- **代码量**：~2000 行 + 测试
- **依赖关系**：
  - `aurora-queue-submit-present` ✅（Submit / Queue / SwapChain 是 RDG 提交的目标）
  - `aurora-encoder-barriers`（CommandBuffer::PipelineBarrier 是 RDG 自动 barrier 的 emit 通道）
  - `aurora-resource-group`（Pipeline / ResourceGroup 是 pass body 内的工作）
  - 三者都是 RDG 的前置；本 change 在它们之后落仓
- **测试**：新增 `RDGTest.cpp`：单 pass、双 pass 链、transient 池命中、import 资源、pass culling、barrier 自动推导（验证 validation layer 不报警）
- **删除**：清理 `aurora/rhi/interface/include/aurora/rdg/RenderGraph.h` / `RenderGraph.cpp` 空 stub（迁到 `aurora/rdg/` 目录）
