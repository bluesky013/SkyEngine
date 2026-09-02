## Context

RDG（Render Dependency Graph / Render Graph）是现代渲染器的标准中间层。参考实现：
- **Frostbite RDG**（Yuriy O'Donnell, GDC 2017）— callback-based，三段式
- **Unreal RDG**（FRDGBuilder）— 类似 Frostbite，加入 async compute 调度
- **bgfx Frame**（隐式 frame graph）— 较薄
- **AMD GPUOpen RPS**（声明式 + 静态分析）— 学院派

本 change 选 **Frostbite-style callback API**：用户写 setup（声明）+ execute（lambda），RDG 内部 compile（图分析）。这套 API 已被工业界反复验证，对 C++ 也最友好。

### 内部结构参考：legacy 的 boost-graph RDG

SkyEngine 旧渲染管线（`engine/render/core/.../rdg/`）里已经有一套成熟的三图 RDG，本 change 的内部结构以它为蓝本：

- **ResourceGraph**：资源节点（image / buffer / view / import），节点间边表示"派生"关系（imageView 派生自 image），每个资源带 `LifeTime { begin, end, reference }`
- **AccessGraph**：pass↔resource 访问图。`AccessPass`（每个 pass 一个节点）+ `AccessRes`（每个资源每次访问一个节点），通过 `nextAccessResID` 链表把同一资源的访问按 pass 顺序串起来
- **RenderGraph**：pass 节点（raster/compute/copy/present/upload/transition），带子结构（subpass / queue / fullscreen）

三图都用 **tag-variant 派发 + 平行数组**（`std::variant<Tag>` 标记类型，`PmrVector<Tag> tags` + `polymorphicDatas` 索引到具体 payload 数组），并用 `boost::dfs_visitor` 做 compile/execute 遍历。

### 与 legacy 的两点关键差异

1. **不引入 boost**。Aurora 层（`engine/aurora/`）当前无 boost 依赖；`boost::adjacency_list` + `boost::dfs_visitor` 是 legacy 的实现细节，本 change 用 hand-rolled 邻接表 + 手写拓扑排序/DFS 替代（见决策 4）。
2. **AccessGraph 需重新评估**（见决策 3）。legacy 的独立 AccessGraph + `AccessRes` 链表节点是 barrier/生命周期推导的载体，但引入一层 `AccessPass`/`AccessRes` 间接 + 复杂的 `examine_edge` 遍历。本 change 保留其**语义**，但把载体折叠为每个资源上的一条连续访问链，去掉独立图。

### Aurora 已有资产

- ✅ Queue / Submit / SwapChain — RDG 提交时调用
- ✅ `CommandBuffer::PipelineBarrier(BarrierInfo)`（aurora-encoder-barriers 已实施）— RDG 自动 barrier 的输出通道；`BarrierInfo` 已聚合 memory/buffer/image 三类 barrier
- ✅ `InferLayoutForAccess(AccessFlags)` / `IsLayoutCompatibleWithAccess`（`aurora/rhi/Barrier.h`）— access → layout 推导
- ✅ `AccessFlags` 已按 stage × R/W × resource 细分，`ImageLayout` / `PipelineStageFlags` / `ImageBarrierInfo` / `BufferBarrierInfo` 现成
- 🟡 `PipelineLayout` / `ResourceGroup`（aurora-resource-group）— pass body 中绑定时用

约束：
- 本 change 不实现 multi-queue 调度（async compute 留 v2）
- 不实现 sub-pass / merge-pass 优化（VK render pass 合并那种，dynamic rendering 时代收益有限）
- persistent（跨帧）资源 v1 只预留 `residency` 字段、不实现分配（persistent 池留 v2）；v1 跨帧需求用 `Import(rhi::Image*)` 导入

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
- 不实现 persistent（跨帧）资源分配——v1 只实现 Transient + Import；`residency` 字段预留，persistent 池留 v2

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

### 决策 2：资源声明与寻址模型

#### 2.1 强类型 opaque handle

```cpp
struct RDGTextureHandle { uint32_t id = INVALID_INDEX; };
struct RDGBufferHandle  { uint32_t id = INVALID_INDEX; };
```

强类型避免误把 buffer 当 texture 用；底层都是 vector index。Handle 只在当前 graph 生命周期内有效。

**Why:** 强类型成本低；调试更友好；运行时检查更便宜（compile 时 vector bounds check 即可）。

#### 2.2 Import（外部所有权）+ residency（生命周期）

`Import*` / `Create*` 是两条正交的轴，不能互相替代：

| 所有权 \ 生命周期 | Transient（帧内、可 alias） | Persistent（跨帧、不 alias 不 cull） |
|---|---|---|
| **外部（Import）** | 极少见 | SwapChain backbuffer、用户 render target |
| **RDG 拥有（Create）** | 默认路径（transient 池） | SceneColor / GBuffer / ShadowMap |

- `Import(name, rhi::Image*)` → 外部所有权；RDG 不分配、不销毁。典型：swapchain backbuffer（RDG 无法"创建"它）。
- `CreateTexture(name, desc)` → RDG 拥有；`desc.residency` 决定生命周期：
  - `Transient`（默认）：帧内，alias-aware 池分配，可 cull（v1 实现）
  - `Persistent`：跨帧，按 name 稳定、不 alias 不 cull（v1 **仅预留字段**，分配实现留 v2）

对应 UE：`RegisterExternalTexture`（Import）、`CreateTexture`（Create）、`ERDGTextureFlags::MultiFrame`（Persistent）。对应 legacy：`ImportImage/Buffer` + `ResourceResidency::TRANSIENT/PERSISTENT`。

**Why:** 取消 Import 会让 swapchain backbuffer 无家可归；取消 residency 则把 persistent 推给"用户自己创建再 Import"，混淆所有权与生命周期两个概念。

#### 2.3 纯 handle-based 寻址，不做 name lookup

主接口只走 handle，setup lambda 通过 capture / builder 拿 handle；**不提供**按名字查资源的语义层（legacy `FindVertex(name)` 那种）。名字只用于调试（`DebugName` / GPU marker）。

将来若需跨模块共享"SceneColor"，用 persistent 资源的 name 作池 key 即可，不引入全局 name lookup。

**Why:** UE 同款（`FRDGTextureRef` 强类型引用，名字仅调试）。name-based 会引入 string hash、拼写冲突、隐式生产/消费耦合（写错名字 = 静默缺依赖）；handle-based 编译期可查。

### 决策 3：内部结构 —— 双图 + 访问链（吸收 AccessGraph）

沿用 legacy 的 tag-variant 派发 + 平行数组，但把三图收敛为**两个逻辑结构**，AccessGraph 折叠为资源上的访问链。

#### 3.1 ResourceGraph —— 资源身份与派生

节点 = 资源。Tag 区分资源类别：

```cpp
struct TransientImageTag {};
struct ImportImageTag    {};
struct ImageViewTag      {};   // 派生自 image（mip/layer 视图）
struct TransientBufferTag{};
struct ImportBufferTag   {};
using ResourceTag = std::variant<TransientImageTag, ImportImageTag, ImageViewTag,
                                 TransientBufferTag, ImportBufferTag>;

using ResourceIndex = uint32_t;

struct ResourceNode {
    std::string name;
    ResourceTag  tag;
    uint32_t     payloadIndex;   // 索引到对应类型的 payload 数组
    LifeTime     lifeTime;       // { firstUsePass, lastUsePass, refCount }
    // 访问链（见 3.2）
    uint32_t     firstAccess;    // 本资源访问链首条记录下标
    uint32_t     accessCount;
};
```

与 legacy 相同，资源按类型分 payload 数组：`PmrVector<GraphImage> images`、`PmrVector<GraphImportImage> importImages`、`PmrVector<GraphBuffer> buffers` … 平行数组 `names[] / tags[] / payloadIndex[] / lifeTime[]`。派生关系（ImageView 派生自 Image）用 `parentIndex` 记在 ImageView payload 上，而非显式边。

`residency`（`ResourceResidency::TRANSIENT/PERSISTENT`）放在各 payload 的 desc 上（对应 legacy `GraphImage.residency`），compile 据此区分 alias / cull 语义；v1 只实现 Transient，Persistent 字段预留。

#### 3.2 访问链 —— 替代 AccessGraph

legacy 的 `AccessGraph` 用 `AccessPass`/`AccessRes` 双类节点 + `nextAccessResID` 链表把"某个资源被哪些 pass 按什么 access 访问"串起来。它承担两个职责：

1. 生命周期推导（`LifeTime.begin/end`）
2. barrier 推导（沿链逐对 `MergeBarrier`）

**本 change 结论**：这两个职责不需要一个独立图，也不需要 `AccessRes` 链表节点。折叠为**每个资源拥有的一条连续访问记录链**：

```cpp
struct AccessRecord {
    PassIndex     pass;        // 哪个 pass
    AccessFlags   access;      // 该次访问的 access flags（已合并同 pass 内多 access）
    ImageSubRange subRange;    // 子资源范围（mip/layer）；buffer 用 offset/range
};
```

所有 `AccessRecord` 存进一个全局 `PmrVector<AccessRecord>` arena，`ResourceNode.firstAccess/count` 指向本资源的那一段。`AddDependency(res, pass, access)` 时：

- 若链尾记录与本次**同一 pass 且 access 一致 且 subRange 相同**（连续 read）→ 就地合并
- 否则追加一条新记录（同 pass 内不同 subRange / 不同 access 拆多条，保证 mip-downsample 这种"读 mip0 + 写 mip1"不被误并）

**Why 吸收而不是保留独立 AccessGraph：**
- legacy 的 `AccessPass`/`AccessRes` 节点各带 `vertexID`/`resID` 回指 + `nextAccessResID` 指针，是一层只为了遍历而存在的间接；`examine_edge` 里嵌套 `std::visit` + `boost::out_edges` 的遍历逻辑复杂难维护
- 访问链语义等价（有序的 `(pass, access, subRange)` 序列），但内存连续、cache 友好、调试直观（一个 vector 里直接看某个资源的访问历史）
- 去掉一张图 + 一类节点 + 一个 `lastAccesses[]` 尾巴指针，实现面显著变小

**Alternatives considered:**
- *保留独立 AccessGraph 原样*：与 legacy 完全一致，但把 boost 换成手写；复杂度没降，违背"重新评估"目标
- *每 pass 存资源列表、每资源存 pass 列表（双邻接）*：双向冗余，且 barrier 需要"资源视角的按序序列"，资源侧列表是必需的，pass 侧列表可有可无 → 只保留资源侧（访问链）

#### 3.3 PassGraph —— pass 节点与子结构

节点 = pass。Tag 区分 pass 类型（v1 只三种）：

```cpp
struct RasterPassTag {};
struct ComputePassTag{};
struct CopyPassTag   {};
using PassTag = std::variant<RasterPassTag, ComputePassTag, CopyPassTag>;

using PassIndex = uint32_t;

struct PassNode {
    std::string name;
    PassTag      tag;
    uint32_t     payloadIndex;   // 索引到 rasterPasses[] / computePasses[] / copyPasses[]
    // 本 pass 触碰的资源（编译期填，用于 pass culling 反向追溯）
    PmrVector<ResourceIndex> resources;
    // pass 间依赖边（由访问链推导，见决策 6）
    PmrVector<PassIndex>     dependsOn;
};
```

平行数组 `names[] / tags[] / payloadIndex[]`；payload 数组 `rasterPasses[] / computePasses[] / copyPasses[]`。RasterPass payload 含 attachments、clear values、viewport/scissor、`RenderingInfo`（Execute 时直接构造）。v1 无 subpass（dynamic rendering），故不像 legacy 有 `RasterSubPass`/`RasterQueue`/`FullScreenBlit` 子结构——那套是旧 RenderPass/FrameBuffer 时代的东西。

**Why:** 保留 legacy"tag-variant + 平行数组"的可扩展性（加 pass/resource 类型只加一个 tag + 一个 payload 数组），但去掉 subpass/queue/fullscreen 等旧渲染模型专属子结构。

### 决策 4：Hand-rolled 邻接表 + 手写拓扑排序/DFS，不引入 boost

legacy 用 `boost::adjacency_list<vecS, vecS, directedS>` 承载边、`boost::dfs_visitor` 做遍历。Aurora 层不引入 boost：

- 依赖边直接存在节点上：`PassNode.dependsOn`（`PmrVector<PassIndex>`）即邻接表；不另建 `graph.outEdges` 结构
- 拓扑排序：Kahn 算法（手写，用 `dependsOn` 算入度）
- 反向追溯（pass culling）：从 export 资源出发，手写迭代 BFS（用 `resources` + 访问链的 writer 反查）
- 生命周期/barrier：直接顺序扫描访问链，无需图遍历

**Why:** boost 是 legacy `render/core` 的依赖，不是 Aurora 的；pass 数 < 100，手写邻接表 + Kahn 复杂度 O(V+E) 足够，且比引入 `boost::graph` 头更轻、编译更快、错误信息更可读。

**Alternatives considered:**
- *引入 boost::adjacency_list*：代码与 legacy 一致、可移植性好，但把 boost 拉进 RHI 层，与"aurora 无 boost"的边界冲突
- *CSR 压缩邻接*：对 <100 pass 的图是过度设计；`PmrVector<PassIndex>` 直存即可

### 决策 5：LifeTime 分析 —— 每条访问链首尾定区间

Compile 阶段对每个资源扫它的访问链：

```
firstUsePass = 访问链[0].pass
lastUsePass  = 访问链[n-1].pass
refCount     = 访问链上 distinct pass 数（或累计引用数）
```

与 spec `aurora-rdg-handles` 的 `(firstUsePass, lastUsePass)` 语义一致。该区间用于：
- transient 池 alias 决策（两个资源区间不重叠 + 同 alloc key 可复用）
- export 资源最终 transition（lastUsePass 是终点）
- pass culling（一个资源无下游消费者 → 其 writer pass 可能被剔除）

**Why:** 比 legacy 的 DFS `UpdateLifeTime`（在 `examine_edge` 里 `std::min/std::max` 累加）更直接——访问链本身就是有序的，首尾即区间，无需边遍历。

### 决策 6：Barrier 推导 —— v1 整资源，v2 TrackSubresource 精确

**v1（整资源粒度）**：每个资源维护**单一 state**（一个 access + 一个 layout）。Compile 顺序扫访问链，相邻两条 access 不同时发一条整资源 barrier：

```cpp
for (i = 0; i+1 < accessCount; ++i) {
    if (records[i].access == records[i+1].access) continue;   // 连续同 access 不发
    emit ImageBarrierInfo{
        .subRange  = 整资源（全 mip/layer/aspect）,
        .srcAccess = records[i].access, .dstAccess = records[i+1].access,
        .oldLayout = InferLayoutForAccess(records[i].access),
        .newLayout = InferLayoutForAccess(records[i+1].access),
        // srcStage/dstStage 由 access 查表（见 AGENTS.md 的 access→stage 配对）
    };
}
```

整资源粒度下相邻记录必然交叠，pairwise 扫描即正确；depth/stencil 也按组合 access（`DEPTH_STENCIL_READ/WRITE`）整体处理，无需 aspect 拆分。

**v2（`TrackSubresource` opt-in）**：资源声明 `trackSubresource=true` 才启用 mip/layer/aspect 精确。此时每个资源维护 run-length 合并的 `(subRange, access)` 状态集合，新访问按 region 更新（避免"相邻求交"在 subresource 交错时漏 barrier，例：p1 写 mip0、p2 写 mip1、p3 读 mip0）：

```cpp
void UpdateState(ResourceNode &res, ImageSubRange R, AccessFlags A) {
    for (auto &[sRange, sAccess] : res.state) {
        ImageSubRange ov = Intersect(sRange, R);      // mip ∩ layer ∩ aspect
        if (ov 空 || sAccess == A) continue;
        emit ImageBarrierInfo{ .subRange = ov, .srcAccess = sAccess, .dstAccess = A, ... };
    }
    // R 更新为 A，run-length 合并
}
```

- **full-range 访问自然完成"合并"**：mip chain 写完采整张、cube 6 face 写完采 cubemap，本质都是声明一个 full-range 访问（读或写），region 更新会逐个把 subresource transition 到目标 layout——**不需要显式 Consolidate 原语**
- aspect（depth/stencil）精确也只在 v2：`aspectMask` 分轨；v1 因 access 已是组合的 `DEPTH_STENCIL_*`，无需拆分

Barrier 聚合进 pass 的 `frontBarriers` / `rearBarriers`。Pass body 内部不再有 barrier（execute lambda 手写 `PipelineBarrier` 未定义）。

**Why:** v1 只做整资源，最简且正确（保守）；v2 用 `TrackSubresource` opt-in 拿最优 layout + 子资源 aliasing。不引入显式 Move/Consolidate 原语。

### 决策 7：Transient 池 —— 对象池（v1）+ 堆池（v2 预留）

transient 资源复用分两层，抽象接口一次性预留：

#### 7.1 对象池（v1）：按完整 desc 复用整对象

```cpp
struct PoolEntry {
    AllocKey key;            // = 完整 desc hash
    rhi::ImagePtr image;     // 已分配并绑定内存的完整 Image
    uint32_t lastFreeFrame;  // LRU 淘汰
};
```

- **AllocKey = 完整 `RDGTextureDesc`**：`(format, width, height, depth, mipLevels, arrayLayers, samples, usage)`；buffer 为 `(size, usage)`。**仅 `(format, width, height, usage)` 不够**——depth（3D）、arrayLayers（cubemap/array）、samples（MSAA）、mipLevels 任一不同，内存分配就不同，会误复用。等价于 legacy `std::hash<GraphImage>` 的字段集合。
- Compile 期按 `firstUsePass` 排序，lifetime 不重叠 + 同 key 复用；未命中新建入池。跨帧 LRU 淘汰。

#### 7.2 堆池（v2 预留）：memory heap 级 aliasing

对象池复用的是**整张 Image 对象**，粒度粗。堆池做**内存级 aliasing**——从大块 device memory 按 offset 放置资源，lifetime 不重叠的资源（乃至 sub-resource）在内存上重叠：

```cpp
struct MemoryHeap {
    rhi::DeviceMemory memory;
    uint64_t size;
    // 空闲区间表 (offset, size)，best-fit + 区间合并
};
struct PlacedResource {
    rhi::ImagePtr image;   // 绑定到 heap 的某 offset
    uint64_t offset; uint64_t size;
};
```

- 收益：省 VRAM 更彻底（sub-resource 级 + 内存级），对应 UE `FRHITransientResourceAllocator` / Vulkan memory aliasing
- 代价：memory placement（`vkBindImageMemory` + offset）、aliasing barrier（复用前从 `UNDEFINED` 重转）、lifetime 精确跟踪；复杂度明显更高
- 本 change **只预留 `TransientPool` 抽象接口**，不实现

**Why:** 两套分层——对象池先拿 30-50% 复用（简单、跨帧稳定），堆池作为 v2 的精细路径（省更多但重）。`TransientPool` 抽象接口一次性预留，避免将来改动外层 `RenderGraph`。

**Alternatives considered:**
- *显式 Move(src,dst)*：可审计，但把复用声明推给用户，负担 > 收益，不采用
- *只做对象池*：够用，但不留堆池扩展点，将来要动外层接口
- *引用计数复用*：不能跨 firstUse-lastUse 区间复用，省的少

### 决策 8：Pass culling 反向追溯

从 "exported" 资源（导入的 SwapChain image / 用户标记 `MarkOfInterest(handle)`）反向 BFS：
- 谁写过这个资源（访问链上写该资源的 pass）→ 那个 pass 是 "live"
- 那个 pass 读过哪些资源（`PassNode.resources`）→ 那些资源的 writer pass 也 live
- 递归

未被任何 live pass 依赖的 pass 在 Compile 期被剔除（execute 阶段跳过）。

**Why:** 标准 RDG 优化；简单的"未读 = 死代码"自动消除。

### 决策 9：模块组织——RDG 作为 RHI 接口扩展，编译器/执行器按后端实现

由于 vk / metal / d3d 的 barrier 与 layout 语义差异，RDG **编译器与执行器按后端各自实现**，公共图分析提取到 RHI 接口层：

```
engine/aurora/
    rhi/
        interface/                          ← Aurora.RHI（RHI 接口 + RDG 公共逻辑）
            include/aurora/rdg/
                RenderGraph.h               // 具体类（持 unique_ptr<RDGBackend>）
                RDGBackend.h                // 抽象接口：CompileBarriers / Execute
                RenderGraphBuilder.h
                RDGHandles.h
                RDGContext.h
                RDGGraph.h                  // ResourceNode / PassNode / AccessRecord / LifeTime
            src/rdg/
                RenderGraph.cpp             // Setup：AddXxxPass / AddDependency（访问链）
                Compile.cpp                 // 共享分析：依赖/拓扑/生命周期/culling/transient + DeriveBarriers
                Execute.cpp                 // ExecutePasses（共享 pass 发射）
                TransientPool.{h,cpp}       // 对象池（共享）
        vulkan/
            src/rdg/VulkanRDGBackend.{h,cpp}     // Vulkan 编译器/执行器
        dx12/
            src/rdg/D3D12RDGBackend.{h,cpp}      // DX12 编译器/执行器
        metal/
            src/rdg/MetalRDGBackend.{h,cpp}      // Metal 编译器/执行器
    core/
        ...
```

- `Device::CreateRDGBackend()`（纯虚）由各后端实现，返回各自的 `RDGBackend` 实现
- `RenderGraph::Build(device)` 构造具体 `RenderGraph`，构造时经 `device->CreateRDGBackend()` 取后端（图本身不多态）
- barrier 推导辅助 `InferLayoutForAccess` / `InferStageForAccess` 在 `aurora/rhi/Barrier.h`（接口层共享）

清理：删除 `aurora/rhi/interface/include/aurora/rdg/RenderGraph.h` 与 `interface/src/rdg/RenderGraph.cpp` 空 stub。

命名空间：**`namespace sky::aurora`**（遵循 aurora AGENTS.md 规则，不新增 `aurora::rdg` 别名）。

**Why:** 图分析（依赖/拓扑/生命周期/culling）与 barrier 的 `BarrierInfo` 结构是后端无关的，放接口层避免 4 份重复；而 barrier 的 stage/layout 推导与执行落点是后端相关的，放各后端，便于将来各自做优化（Vulkan 的 `vkCmdPipelineBarrier2` 批量、Metal 的 encoder 边界隐式同步、DX12 的 state promotion/decay elision）。

### 决策 10：Execute 阶段一次 build 一个 CommandBuffer

每次 `graph->Execute(cmdBuf)` 把整个 graph 的 barrier + encoder + pass body 全部 emit 到一个 cmdbuf 里。调用方负责 Submit。多 cmdbuf / multi-queue 是 v2。

```cpp
void RenderGraph::Execute(CommandBuffer *cmdBuf) {
    for (auto &pass : livePasses) {          // 拓扑排序后的 live pass
        cmdBuf->PipelineBarrier(pass.frontBarriers);
        if (pass.type == GRAPHICS) {
            auto enc = cmdBuf->CreateGraphicsEncoder();
            enc->BeginRendering(pass.renderingInfo);
            pass.executeFn(*enc, ctx);
            enc->EndRendering();
        } else if (pass.type == COMPUTE) { ... }
        else if (pass.type == COPY) { ... }
        cmdBuf->PipelineBarrier(pass.rearBarriers);
    }
    cmdBuf->PipelineBarrier(finalBarriers);   // export 资源最终 transition
}
```

**Why:** 与 spec `Execute 输出到单一 CommandBuffer` 一致；v2 再上多 cmdbuf / multi-queue。

### 决策 11：每帧 build vs 持久化

每帧调用方重新 `Build()` + `AddPass(...)` + `Compile()` + `Execute()`。这是 Frostbite/Unreal 的 fast path。

**Why:** 每帧 build 让 conditional pass（"如果开了 SSR 就加这条 pass"）极简；compile 开销可控（pass 数 < 100，复杂度 O(N²) 也只有 10K 操作）。

性能优化（pipeline cache、persistent transient pool 跨帧）独立于 build 模型；池在 RenderGraph 外（属于 device-级），跨帧累计命中。

## Risks / Trade-offs

- **Build 开销** → 每帧 ~50 个 pass 的 compile 应在亚毫秒级；如成 hot path，再加 cache
- **Transient pool 命中率** → 同 (extent, format, usage) 的 texture 跨帧应稳定命中；不稳定时退化为创建新；用 hit/miss 计数监测
- **不支持 async compute** → 简化执行模型；后续 v2 加 multi-queue + 跨队列 timeline semaphore（Vulkan/Metal 现成、DX12 也现成）
- **Barrier 推导 mip/layer 粒度保守** → v1 整资源（可能锁 `GENERAL`）；v2 `TrackSubresource` 精确 + 子资源 aliasing
- **访问链折叠掉 AccessGraph 的表达力** → 若未来需要 sub-resource 级别访问跟踪，访问链可在 `AccessRecord` 上扩展 `subRange` 细分，无需回退到独立图
- **空 stub `aurora/rhi/interface/.../rdg/RenderGraph.{h,cpp}` 删除影响** → 当前没有 include；安全
- **`CommandBuffer::PipelineBarrier` 依赖** → aurora-encoder-barriers 已实施 ✅；`InferLayoutForAccess` 现成 ✅

## Migration Plan

依赖关系（落仓顺序）：
1. `aurora-queue-submit-present` ✅（Submit + SwapChain 都通）
2. `aurora-encoder-barriers` ✅（`CommandBuffer::PipelineBarrier` 已实施）
3. `aurora-resource-group`（pass body 内部用；RDG 不阻塞于此，可并行）
4. **本 change**

落仓时：
1. 先建 `engine/aurora/rdg/` 目录与 CMakeLists
2. 接口头先合（`RDGHandles.h` / `RDGGraph.h` / `RenderGraphBuilder.h` / `RenderGraph.h`）
3. 实现 + 测试一并落（`RenderGraph.cpp` / `Compile.cpp` / `Execute.cpp` / `TransientPool.cpp`）
4. 删除老的空 stub

## Open Questions

- **Pass execute lambda 的 capture 安全性？** 用户 lambda 持有 scene 引用、camera 等。Aurora 不强制 lifetime；交给上层（Renderer）。本 change 文档化"execute lambda 必须在 Execute() 调用结束前持有所有 captured 引用"。
- **是否提供 Pass 内的 sub-Builder（可在 execute 中再插 pass）？** Frostbite 有，Unreal 有。倾向：本 change 不做；execute 内不能再加 pass。如有动态需求，调用方在 setup 阶段用 if 控制即可。
- **Transient pool 是 device-级还是 graph-级？** Device-级跨帧累积；graph-级每次重置。倾向：device-级（持有于 RDGSystem 单例或 device 扩展），跨帧复用 image。
- **AccessFlags / Layout 推导能否做"同 stage 多 read 合并"？** 例如 frag shader read + vertex shader read 的同一 texture，可合并成单 SHADER_READ_ONLY。倾向：`AddDependency` 在链尾同 pass 合并时做 OR 合并；deeper 优化留后续。
- **访问链的内存来源？** 与 legacy 一致用 frame arena（`PmrVector` + 帧内 `LinearStorage`），每帧 build 时重置，避免每次 `AddDependency` 的堆分配抖动。
