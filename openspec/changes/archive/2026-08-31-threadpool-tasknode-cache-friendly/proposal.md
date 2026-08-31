## Why

ThreadPool 的依赖图任务节点 `TaskNode` 存储对 cache 不友好:每个节点由 `new` 独立分配导致内存分散,`std::vector children`、`std::promise`、`std::mutex` 各带来额外堆分配,节点整体膨胀到 ~200+ 字节(跨 3~4 条 cache line)。同时 TaskNode 任务全部汇入单一 MPMC `globalQueue`,未利用已有的 per-thread work-stealing 机制。渲染图的依赖调度是帧内热路径,优化可降低分配次数并提升调度吞吐。

## What Changes

- 为 `TaskNode` 引入 arena/slab 分配器,按提交批次连续分配节点,提升空间局部性(替代逐节点 `new`)。
- 用 small-vector(内联 2~4 槽)替换 `children` 的 `std::vector`,避免常见 1~2 子节点场景的堆分配。
- 将 `std::mutex childMutex` 替换为轻量 spinlock(或原子方案),缩小节点体积并降低锁开销。
- 移除每节点 `std::promise` 的强制堆分配(按需/共享 completion 计数)。
- TaskNode 依赖图任务下沉到 local queue + work-stealing,避免 globalQueue 单点竞争。
- 缩小任务可调用体(减少 `std::function` 间接调用/膨胀)。

## Capabilities

### New Capabilities

- `async-threadpool`: ThreadPool 任务调度与 TaskNode 依赖图节点的存储/分配策略,及其 cache 友好性契约。

### Modified Capabilities

<!-- 无现有 capability 的 spec-level 行为变化 -->

## Impact

- **代码**: `engine/core/include/core/async/ThreadPool.h`、`engine/core/src/async/ThreadPool.cpp`(TaskNode 布局、分配器、调度路径);可能新增 arena allocator 辅助类型。
- **依赖**: 复用 `core/template`(ReferenceObject、LockFreeQueue),无新增第三方库。
- **测试**: `engine/test/core/ThreadPoolTest.cpp`(TaskNode 线性链/菱形/多根依赖测试)须保持通过;可新增分配次数/局部性相关测试。
- **API**: `CreateTask`/`Submit`/`DependsOn` 对外签名不变;`GetFuture()` 语义保持。
