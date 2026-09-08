## Context

当前引擎内存分配碎片化：

- **旧 render RDG**（`engine/render/core/rdg/`）：全 PMR（`PmrVector`/`PmrUnSyncPoolRes`），`memory_resource` 虚调用 + `polymorphic_allocator` 类型污染，且 `RasterQueue.renderItems` 是 `PmrList`。
- **aurora RDG**（`engine/aurora/rhi/interface/rdg/`）：默认 `std::allocator`，每帧建图 = 数千次 malloc/free + `std::function` lambda capture 分配。
- **core 层**：`TransientAllocator`（bump + `TransientStdAllocator`）已存在且测试完备，但缺 `Mark`/`Rewind`、`TransientVector`、统一门面、统计。

## Goals / Non-Goals

**Goals:**

- `LinearStorage` 支持 `Mark`/`Rewind`/`GetMark`，支持跨 block 回退。
- `TransientAllocator` 透传检查点 API；补 `TransientVector`/`MakeTransientVector`。
- 新增 `FrameAllocator`：非虚 bump + `Mark`/`Rewind` + `Arena()` 入口 + bytes/peak 原子统计。
- aurora RDG setup graph 迁移到 `FrameAllocator`；`Compile` 产出独立 `CompiledGraph`。
- `DeviceFrameContext` 持有 `FrameAllocator`，帧末统一 `Reset()`。

**Non-Goals:**

- 不引入线程安全锁/TLS（单线程约定）。
- 不做 per-allocation 元数据/调用栈统计。
- 不改旧 render RDG 的 PMR 路径（legacy，后续统一迁移）。
- 不引入 size-class pool（bump+rewind 已覆盖阶段式回收；若未来出现交错 free 再评估）。

## Decisions

### 1. bump+rewind 而非 pool

setup graph 每帧「建 → 编译 → 丢弃」，无单对象 free 需求。bump 分配 O(1) 无元数据；`Rewind(mark)` 一条指令整体回收。pool 的 free list/header/size-class 碎片在此是纯开销。若未来出现「帧内交错不同生命周期 alloc/free」再评估 pool。

### 2. 复用 `TransientAllocator` 而非新写 bump

`TransientAllocator` 已封装 `LinearStorage` 的块链管理 + `TransientStdAllocator` 适配器 + 测试。`FrameAllocator` 做薄封装：持有 `LinearStorage` + 通过 external-storage 构造的 `TransientAllocator` 视图，透传 `Mark`/`Rewind`/`Arena()`。

### 3. 统计仅 bytes/peak/count

`Allocate` 时 `mCurrentBytes += size; mPeakBytes = max(mPeakBytes, mCurrentBytes); ++mAllocationCount`；`Rewind` 回退 `mCurrentBytes`；`Reset` 清零 `mCurrentBytes` 并回退 peak。无 per-allocation 记录，零元数据。

### 4. 单线程，预留扩展点

`FrameAllocator` 文档注明「单线程使用」。若 `DeviceFrameDispatcher` 执行期 worker 需分配，后续加 TLS arena 或 spinlock 分片，当前不加锁。

### 5. CompiledGraph 独立且扁平

`Compile` 从同一 `FrameAllocator` bump 出 `CompiledGraph`：扁平 `CompiledPass[]`（只含 live，topo 序）、每 pass 一段连续 `BarrierInfo[]`、全局 `Image*[]`/`Buffer*[]` 解析表。setup graph 编译后立即 `Rewind` 回收。

## Risks / Trade-offs

- **[setup graph 编译前被 Rewind]** `CompiledGraph` 必须持有所有 executor 需要的数据，不能引用 setup graph。→ 编译时显式拷贝到 compiled 结构。
- **[`TransientStdAllocator` 绑定 arena 悬垂]** 持久结构不得绑定帧 arena。→ `FrameAllocator` 文档 + RDG 接口隔离。
- **[Mark/Rewind 跨 block 实现]** `Rewind` 需丢弃 mark 之后的 block 并回退 mark 所在 block offset。→ `LinearStorage` 内部 block 索引 + offset 记录。

## Migration Plan

1. core 层：补 `LinearStorage::Mark/Rewind` + `TransientAllocator` 透传 + `TransientVector` + `FrameAllocator` + 统计 + 测试。
2. aurora RDG：`RenderGraph` 字段迁移到 `TransientVector`；`Build` 加 `FrameAllocator&`；`Compile` 产出 `CompiledGraph`。
3. `DeviceFrameContext` 持有 `FrameAllocator`；`RenderDeviceExclusive::BeginFrame/EndFrame` 调 `Reset`。
4. 测试：`LinearStorage`/`TransientAllocator`/`FrameAllocator` 新增 `Mark/Rewind`/统计测试；RDGTest 适配。
5. archive 本 change。
