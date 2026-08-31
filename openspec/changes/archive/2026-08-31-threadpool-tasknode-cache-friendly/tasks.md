## 1. 节点存储优化(arena + small-vector + spinlock)

- [x] 1.1 新增轻量 arena/slab 分配器(如 4KB 块链 monotonic allocator)于 `core/template` 或 `core/async` 内部
- [x] 1.2 为 `TaskNode` 接入 arena 分配(`CreateTask` 不再逐节点 `new`),并处理批次回收时机
- [x] 1.3 用内联 4 槽 small-vector 替换 `children` 的 `std::vector`
- [x] 1.4 用轻量 spinlock 替换 `childMutex`
- [x] 1.5 跑通 `ThreadPoolTest` 的 `TaskNode*` 系列测试,确认行为不变

## 2. 节点瘦身(promise 按需 + 可调用体瘦身)

- [x] 2.1 `std::promise` 改为惰性分配,`GetFuture()` 首次访问才分配 shared_state
- [x] 2.2 `TryEnqueue` 构造的 lambda 移除 `CounterPtr` 捕获,改持裸指针(依赖 arena 生命周期)
- [x] 2.3 跑通 `ThreadPoolTest` 与 future 相关断言

## 3. 调度路径优化(TaskNode 走本地队列)

- [x] 3.1 `Enqueue` 改为按 worker round-robin 推入 `localQueues`
- [x] 3.2 确认 `Dispatch` 保留走 globalQueue 的既有语义
- [x] 3.3 跑通 `Parallel*`、`TaskNode*`、`WaitIdleDrainsAllTasks` 及并发回归测试

## 4. 验证与收尾

- [x] 4.1 新增/调整测试覆盖:连续分配相邻性、小 fan-out 零堆分配、future 惰性分配
- [x] 4.2 运行 `engine/test/core/ThreadPoolTest` 全量,确认无退化
- [x] 4.3 按仓库 clang-format/clang-tidy 规范格式化改动文件
