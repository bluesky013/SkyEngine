## Context

RDG（Render Dependency Graph / Render Graph）是现代渲染器的标准中间层。参考实现：
- **Frostbite RDG**（Yuriy O'Donnell, GDC 2017）— callback-based，三段式
- **Unreal RDG**（FRDGBuilder）— 类似 Frostbite，加入 async compute 调度
- **bgfx Frame**（隐式 frame graph）— 较薄
- **AMD GPUOpen RPS**（声明式 + 静态分析）— 学院派

本 change 选 **Frostbite-style callback API**：用户写 setup（声明）+ execute（lambda），RDG 内部 compile（图分析）。这套 API 已被工业界反复验证，对 C++ 也最友好。

Aurora 已有的资产：
- ✅ Queue / Submit / SwapChain — RDG 提交时调用
- 🟡 CommandBuffer::PipelineBarrier（aurora-encoder-barriers 中改设计） — RDG 自动 barrier 的输出通道
- 🟡 PipelineLayout / ResourceGroup（aurora-resource-group） — pass body 中绑定时用
- ✅ AccessFlags 已按 stage × R/W × resource 细分 — RDG barrier 推导直接用

约束：
- 本 change 不实现 multi-queue 调度（async compute 留 v2）
- 不实现 sub-pass / merge-pass 优化（VK render pass 合并那种，dynamic rendering 时代收益有限）
- 不实现跨帧资源（persistent textures）—— 用户用 `Import(rhi::Image*)` 导入

## Goals / Non-Goals

**Goals:**
- 用户可声明 N 个 pass，每个 pass 描述读写哪些资源 + 一段 execute lambda
- RDG compile 自动：拓扑排序、生命周期分析、transient 池分配、barrier 推导、pass culling
- RDG execute 把 barrier + pass 顺序 emit 到 CommandBuffer
- Transient 资源池化：相同 (extent, format, usage) 的 image 在生命周期不重叠时复用底座
- 端到端测试：3-pass 链（compute write → graphics read → present）跑通，validation 不报警

**Non-Goals:**
- 不做 async compute 跨队列调度（仅 graphics queue）
- 不做 sub-pass / 多 pass 合并到单 render pass
- 不做 GPU readback（pass body 的 readback 由用户自行 Submit 一段 transfer）
- 不做 persistent state cache（每帧重新 build 整个 graph 是 fast path）
- 不实现 shader/pipeline 自动管理（pass body 自己创建/缓存 pipeline）
- 不实现 resource state tracking 跨 frame（每帧 graph 内部独立）

## Decisions

### 决策 1：Callback-based API + 三段式 lifecycle

```cpp
auto graph = RenderGraph::Build(device);

auto albedo = graph->CreateTexture("albedo", {1920, 1080, BGRA8_UNORM, RENDER_TARGET | SAMPLED});
auto depth  = graph->CreateTexture("depth",  {1920, 1080, D32_S8, DEPTH_STENCIL | SAMPLED});
auto bbImg  = graph->Import("backbuffer", swapchain->GetImage(idx));

graph->AddRasterPass("gbuffer",
    [&](RasterPassBuilder &b) {
        b.ColorAttachment(0, albedo, LoadOp::CLEAR, StoreOp::STORE);
        b.DepthStencilAttachment(depth, LoadOp::CLEAR, StoreOp::STORE,
                                        LoadOp::DONT_CARE, StoreOp::DONT_CARE);
    },
    [scene](GraphicsEncoder &enc, RDGContext &ctx) {
        scene->RenderGBuffer(enc);
    });

graph->AddRasterPass("present",
    [&](RasterPassBuilder &b) {
        b.Read(albedo, AccessFlagBit::FRAGMENT_SRV);
        b.ColorAttachment(0, bbImg, LoadOp::CLEAR, StoreOp::STORE);
    },
    [](GraphicsEncoder &enc, RDGContext &ctx) {
        // sample albedo, render fullscreen quad to bb
    });

graph->Compile();
graph->Execute(cmdBuf);
```

**Why:** Frostbite/Unreal 的事实标准；C++ lambda 友好；setup 与 execute 分离让 compile 阶段可以全局分析。

### 决策 2：Resource handles 是按 type 强类型 opaque ID

```cpp
struct RDGTextureHandle { uint32_t id = INVALID_INDEX; };
struct RDGBufferHandle  { uint32_t id = INVALID_INDEX; };
```

强类型避免误把 buffer 当 texture 用；底层都是 vector index。Handle 只在当前 graph 生命周期内有效。

**Why:** 强类型成本低；调试更友好；运行时检查更便宜（compile 时 vector bounds check 即可）。

### 决策 3：Transient 池分配用简化 alias-aware

每个 transient texture 按 `(width, height, depth, format, samples, usage)` hash 出 alloc key。RDG compile 计算每个 handle 的 (firstUsePass, lastUsePass) 区间。两个区间不重叠 + 同 alloc key 的可以共享同一个底层 RHI Image。

```cpp
struct PoolEntry {
    AllocKey key;
    rhi::ImagePtr image;
    uint32_t lastFreeFrame;     // for LRU eviction
};
```

每次 Compile 时，对所有 transient texture 按 firstUsePass 排序，逐个找池中：
1. 同 alloc key
2. 已被释放（pass 已结束 lifetime）

的 entry 复用；找不到则创建新 image 入池。

简化策略不实现 sub-resource aliasing（同一物理 image 切多个 transient view），那是 v2 优化。

**Why:** 简单实现已能省 30-50% transient VRAM；sub-resource aliasing 复杂度跳跃太大，留后续。

**Alternatives considered:**
- *VK_KHR_external_memory + 手动 alloc*：高级特性，依赖 driver；本 change 用纯 RHI Image 池
- *引用计数复用*：不能跨 firstUse-lastUse 区间复用，省的少

### 决策 4：Barrier 推导：每条 (writer→reader) 边一组 barrier

Compile 阶段对每个资源算"使用序列"：[(pass_i, access_i, layout_i), ...]。相邻两次使用之间发一组 barrier：`srcAccess=prev.access, dstAccess=next.access, oldLayout=InferLayoutForAccess(prev.access), newLayout=InferLayoutForAccess(next.access), srcStage/dstStage from access`。

第一次使用前从 `UNDEFINED` 转入；导出资源（如 SwapChain image）最后一次使用后转 `PRESENT`。

Pass body 内部不再有 barrier（pass 间所有 barrier 由 RDG 提供）。如果用户在 execute lambda 内手写 `cmdBuf->PipelineBarrier`，行为未定义（debug 下 RDG 可以记录"已发"barrier 检测重复）。

### 决策 5：Pass culling 反向追溯

从 "exported" 资源（导入的 SwapChain image / 用户标记 `MarkOfInterest(handle)`）反向 BFS：
- 谁写过这个资源 → 那个 pass 是 "live"
- 那个 pass 读过哪些资源 → 那些资源的 writer pass 也 live
- 递归

未被任何 live pass 依赖的 pass 在 Compile 期被剔除（execute 阶段跳过）。

**Why:** 标准 RDG 优化；简单的"未读 = 死代码"自动消除。

### 决策 6：模块组织——Aurora.RDG 独立静态库

```
engine/aurora/
    rhi/
        interface/   → Aurora.RHI
        vulkan/      → AuroraVulkan (dlopen)
        metal/       → AuroraMetal (dlopen)
        ...
    rdg/                                    ← 新建
        include/aurora/rdg/
            RenderGraph.h
            RenderGraphBuilder.h
            RDGHandles.h
            RDGContext.h
        src/
            RenderGraph.cpp
            Compile.cpp
            Execute.cpp
            TransientPool.cpp
        test/
            RDGTest.cpp
        CMakeLists.txt → Aurora.RDG (STATIC, links Aurora.RHI)
    core/
        ...                                  → Aurora.Core (top-level Renderer 用 RDG)
```

清理：删除 `aurora/rhi/interface/include/aurora/rdg/RenderGraph.h` 与 `interface/src/rdg/RenderGraph.cpp` 空 stub。

**Why:** RDG 不属于 RHI 接口层（依赖 RHI 但不被 RHI 后端依赖）。独立静态库让 4 个 backend dylib 不强制链 RDG。

### 决策 7：Execute 阶段一次 build 一个 CommandBuffer

每次 `graph->Execute(cmdBuf)` 把整个 graph 的 barrier + encoder + pass body 全部 emit 到一个 cmdbuf 里。调用方负责 Submit。多 cmdbuf / multi-queue 是 v2。

```cpp
void RenderGraph::Execute(CommandBuffer *cmdBuf) {
    for (auto &pass : livePasses) {
        cmdBuf->PipelineBarrier(pass.preBarrier);
        if (pass.type == GRAPHICS) {
            auto enc = cmdBuf->CreateGraphicsEncoder();
            enc->BeginRendering(pass.renderingInfo);
            pass.executeFn(*enc, ctx);
            enc->EndRendering();
        } else if (pass.type == COMPUTE) { ... }
        else if (pass.type == COPY) { ... }
    }
    // final barriers (e.g. last write → PRESENT for swapchain image)
    cmdBuf->PipelineBarrier(finalBarriers);
}
```

### 决策 8：每帧 build vs 持久化

每帧调用方重新 `Build()` + `AddPass(...)` + `Compile()` + `Execute()`。这是 Frostbite/Unreal 的 fast path。

**Why:** 每帧 build 让 conditional pass（"如果开了 SSR 就加这条 pass"）极简；compile 开销可控（pass 数 < 100，复杂度 O(N²) 也只有 10K 操作）。

性能优化（pipeline cache、persistent transient pool 跨帧）独立于 build 模型；池在 RenderGraph 外（属于 device-级），跨帧累计命中。

## Risks / Trade-offs

- **Build 开销** → 每帧 ~50 个 pass 的 compile 应在亚毫秒级；如成 hot path，再加 cache
- **Transient pool 命中率** → 同 (extent, format, usage) 的 texture 跨帧应稳定命中；不稳定时退化为创建新；用 hit/miss 计数监测
- **不支持 async compute** → 简化执行模型；后续 v2 加 multi-queue + 跨队列 timeline semaphore（Vulkan/Metal 现成、DX12 也现成）
- **Barrier 推导可能保守**（如对同一资源连续读 read 之间也插 barrier） → 优化：合并连续 read 的 access masks，仅在 write 边界发 barrier
- **空 stub `aurora/rhi/interface/.../rdg/RenderGraph.{h,cpp}` 删除影响** → 当前没有 include；安全
- **CommandBuffer::PipelineBarrier 还没实现** → 本 change 严格依赖 aurora-encoder-barriers；落仓顺序必须先 barriers 后 RDG

## Migration Plan

依赖关系（落仓顺序）：
1. `aurora-queue-submit-present` ✅（已 30/43，Submit + 3/4 SwapChain 都通）
2. `aurora-encoder-barriers`（待开发；CommandBuffer::PipelineBarrier 是 RDG 的输出通道）
3. `aurora-resource-group`（待开发；pass body 内部用）
4. **本 change**

落仓时：
1. 先建 `engine/aurora/rdg/` 目录与 CMakeLists
2. 接口头先合
3. 实现 + 测试一并落
4. 删除老的空 stub

## Open Questions

- **Pass execute lambda 的 capture 安全性？** 用户 lambda 持有 scene 引用、camera 等。Aurora 不强制 lifetime；交给上层（Renderer）。本 change 文档化"execute lambda 必须在 Execute() 调用结束前持有所有 captured 引用"。
- **是否提供 Pass 内的 sub-Builder（可在 execute 中再插 pass）？** Frostbite 有，Unreal 有。倾向：本 change 不做；execute 内不能再加 pass。如有动态需求，调用方在 setup 阶段用 if 控制即可。
- **Transient pool 是 device-级还是 graph-级？** Device-级跨帧累积；graph-级每次重置。倾向：device-级（持有于 RDGSystem 单例或 device 扩展），跨帧复用 image。
- **AccessFlags / Layout 推导能否做"同 stage 多 read 合并"？** 例如 frag shader read + vertex shader read 的同一 texture，可合并成单 SHADER_READ_ONLY。倾向：本 change 做这一层简单合并；deeper 优化留后续。
