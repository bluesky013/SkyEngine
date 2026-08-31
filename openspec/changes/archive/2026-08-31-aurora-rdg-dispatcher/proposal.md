## Why

RDG Compile 期的拓扑排序、barrier 推导、生命周期分析是 CPU 密集、且子任务之间存在依赖关系的工作。若单线程串行执行，pass 数较多时成为每帧 hot path。需要一个轻量依赖图调度器把这些子任务并行化到多核 worker 上。

现有 `ThreadPool::TaskNode` 是流式 API（边建边提交），每个节点带引用计数 + 自旋锁，节点约 128B。而 RDG Compile 是「单线程构建、批次提交、整体释放」的天然场景——构建期单线程、执行期只读，引用计数与锁这些运行时开销完全可以省掉。

## What Changes

- 新增 `FrameGraphDispatcher`（`sky::aurora`）：单线程构建、批次提交、无锁无引用计数的依赖图调度器
- 节点用 `NodeIndex`（`uint32_t`）标识，`children` 存 index（4B/边），节点连续存储于 `std::vector`
- `pendingParents` 构建期用普通 `std::vector<uint32_t>`（单线程累加），`Submit` 时一次性冻结成连续 `std::atomic_uint32_t` 数组供 worker 并发递减
- `Submit` 先收集全部根节点再统一入队（避免入队扫描与执行期 `fetch_sub` 并发导致非根节点被误判为根）
- 经 `ThreadPool::Schedule`（round-robin + work-stealing）并行执行
- 提供 per-node future（构建期获取）与批次级 future（`Submit` 返回）

## Capabilities

### New Capabilities

- `aurora-rdg-dispatcher`: 单线程构建 + 批次提交的并行依赖图调度器

### Modified Capabilities

（无既有 spec 修改）

## Impact

- **新文件**：`aurora/rhi/interface/include/aurora/rdg/FrameGraphDispatcher.h` / `interface/src/rdg/FrameGraphDispatcher.cpp`
- **测试**：`aurora/rhi/test/FrameGraphDispatcherTest.cpp`（线性链 / diamond / 多根 / 空批次 / 复用 / future）+ `FrameGraphDispatcherBenchmark.cpp`（对比 `ThreadPool::TaskNode`）
- **依赖**：`core/async/ThreadPool` 新增 public `Schedule(ThreadTask&&)`（round-robin 入队）
- **性能**：依赖图吞吐约比流式 `TaskNode` 高 ~28%（无引用计数、无锁、节点更小）
