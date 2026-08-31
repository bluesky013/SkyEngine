## Context

`TaskNode` 是 ThreadPool 的依赖图节点(ThreadPool.h:40-62),用于渲染图等帧内依赖调度。当前实现每个节点:

- 由 `CreateTask` 逐节点 `new`(ThreadPool.cpp:127),节点内存分散。
- `children` 用 `std::vector<CounterPtr<TaskNode>>`(ThreadPool.h:61),每节点至少一次堆分配。
- `childMutex` 为 `std::mutex`(ThreadPool.h:60),Windows 上 ~80B,inline 使节点膨胀。
- `std::promise<void>`(ThreadPool.h:58)每节点分配 shared_state。
- `ThreadTask`(`std::function`,32~64B)与 `TryEnqueue` 捕获 `self` 的 lambda 带来间接调用与额外引用计数。
- `Enqueue` 只推 `globalQueue`(ThreadPool.cpp:137-144),TaskNode 任务不走 local queue/work-stealing,形成单一 MPMC 队列竞争点;而 `Parallel` 已走 local queue round-robin + `TrySteal`(ThreadPool.cpp:146-183)。

约束:依赖图节点在构建期(单线程)`DependsOn` 添加子节点,执行期 `TryEnqueue` 才并发消费 `children`;`GetFuture()`/`Submit`/`DependsOn` 对外语义须保持不变。

## Goals / Non-Goals

**Goals:**

- 减少 TaskNode 构建/调度路径的堆分配次数(目标:常见 1~2 子节点场景做到每节点 1 次 arena 分配)。
- 缩小节点体积、提升节点访问的空间局部性。
- 让 TaskNode 依赖图任务复用 local queue + work-stealing,消除 globalQueue 单点竞争。
- 保持 `ThreadPool`/`TaskNode` 对外 API 与现有测试语义不变。

**Non-Goals:**

- 不改 `LockFreeQueue` 的 MPMC 算法本身(作为既有基础设施)。
- 不改 `CounterPtr`/`RefObject` 的引用计数模型。
- 不引入第三方内存池库;arena 用仓库内自研轻量类型实现。
- 不做跨平台的精确 cache line 基准(nanobench 由后续测量验证,非本 change 硬性要求)。

## Decisions

### 决策 1:TaskNode 用 arena/slab 连续分配

`CreateTask` 改用 per-submit-batch 的 monotonic arena(固定块 slab,如 4KB 块链)分配节点,替代逐节点 `new`。节点通过自定义 `operator new/delete` 或工厂函数从 arena 取内存。

- 理由:节点在依赖图中按创建顺序紧密排列,`children` 遍历命中同一 cache line 概率高;一次提交批次后整体释放,免去逐节点 free。
- 备选:`std::shared_ptr` 池化、`std::pmr::monotonic_buffer_resource`。前者语义变化大,后者分配单元仍不保证相邻节点布局可控。选自研 arena 以精确控制对齐与批释放。

### 决策 2:children 用 small-vector(内联槽)

`children` 替换为内联 4 槽的 small-vector(对象内 `CounterPtr<TaskNode> storage[4]` + 溢出才堆分配),替代 `std::vector`。

- 理由:渲染图节点典型 1~2 子节点,内联消除每节点堆分配与一次指针跳转。
- 备选:`std::inplace_vector`(C++26,不可用);固定数组上限(有溢出风险)。选 small-vector 折中。

### 决策 3:childMutex 换 spinlock

`childMutex` 替换为轻量自旋锁(原子 flag + `yield` 退避),仅保护 `children` 的构建期写入与执行期搬移(临界区极短,仅 `emplace_back`/`std::move`)。

- 理由:临界区纳秒级,`std::mutex` 的 futex 系统调用与 ~80B 体积不划算;spinlock 体积 ~1B,临界区短无公平性诉求。
- 备选:无锁 intrusive list(复杂度高,构建期还需处理并发 `DependsOn`)。选 spinlock 保持逻辑简单。

### 决策 4:promise 按需分配

`GetFuture()` 未调用时不为节点构造 `std::promise`。改为惰性创建(`std::unique_ptr<std::promise<void>>`,或节点内 union/optional),`GetFuture()` 首次访问才分配。

- 理由:依赖图内部节点多不取 future,省去每节点 shared_state 分配。
- 备选:共享全局 completion 信号量(需改 future 语义)。选按需分配保持 `std::future<void>` 返回类型不变。

### 决策 5:TaskNode 任务下沉 local queue

`Enqueue` 改为按 worker 轮转推入对应 `localQueues`,worker 主循环已有的 local→steal→global 优先级不变。`Submit` 的根节点/无父节点任务亦走 local queue。

- 理由:复用 `TrySteal` 负载均衡,消除 globalQueue 单点竞争;`Parallel` 已验证该路径正确性。
- 备选:保留 globalQueue 但按 hash 分片。改动更大且不解决窃取收益。选复用 local queue。
- 注意:`Dispatch`(单任务)保留走 globalQueue 的语义,避免改变其既有行为。

### 决策 6:缩小可调用体

`TryEnqueue` 构造的 lambda 不再捕获 `CounterPtr<TaskNode> self`,改为捕获裸指针(依赖 arena 批次生命周期保证执行前不释放)或直接内联 `func` 移动进队列,减少 `std::function` 膨胀与一次 AddRef/RemoveRef。

- 理由:任务执行期间节点必被批次 arena 持有,裸指针安全;去掉 shared-state 增删。
- 备选:继续 `CounterPtr` 捕获(最安全,但保留额外原子操作)。选裸指针 + 生命周期契约,由 arena 保证。

## Risks / Trade-offs

- [arena 生命周期与 `GetFuture()`/外部持节点跨批次] → arena 按"Submit 批次 + 所有未来已消费"判定释放;或退回逐节点 free 作为安全兜底,需在 `Submit`/`WaitIdle` 处保证任务执行完毕才回收。
- [spinlock 在高度竞争下可能自旋浪费] → 临界区仅 `emplace_back`/`std::move`,写入方少;必要时退避 `std::this_thread::yield()`。
- [small-vector 溢出路径仍堆分配] → 溢出时退化为堆数组,行为正确性不变,仅少部分大 fan-out 节点退化为原路径。
- [TaskNode 走 local queue 后窃取顺序变化] → 依赖关系由 `pendingParents` 计数保证,与队列分布无关,正确性不变;负载均衡需回归测试确认不退化。

## Migration Plan

1. 先落 arena + small-vector + spinlock(纯节点存储优化,无调度行为变化),跑通 ThreadPoolTest。
2. 再落 promise 按需分配与 lambda 瘦身。
3. 最后切 TaskNode 调度到 local queue,跑 `Parallel`/`TaskNode*` 系列测试与 `WaitIdle` 回归。
4. 全程以 `engine/test/core/ThreadPoolTest.cpp` 为回归基准,必要时用 per-test 断言校验分配次数。

## Open Questions

- arena 单块大小(建议 4KB)与是否按线程池分片,待实现阶段测量后定。
- spinlock 是否需要 `pause`/`yield` 分级退避,由争用测试决定。
