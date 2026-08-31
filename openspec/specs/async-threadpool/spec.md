# async-threadpool Specification

## Purpose
TBD - created by archiving change threadpool-tasknode-cache-friendly. Update Purpose after archive.
## Requirements
### Requirement: TaskNode 连续分配

`ThreadPool` SHALL 通过 arena/slab 分配器(而非逐节点 `new`)为 `TaskNode` 分配内存,使同一提交批次内创建的节点在内存中连续排列。分配器 SHALL 在批次任务全部执行完毕后统一回收节点内存。

#### Scenario: 连续创建的节点内存相邻

- **WHEN** 连续调用 `CreateTask` 创建多个 `TaskNode` 并 `Submit`
- **THEN** 节点对象地址 SHALL 落在同一 arena 块内(相邻),而非每次独立堆分配

#### Scenario: 批次执行后内存回收

- **WHEN** 某提交批次的所有 `TaskNode` 及其 future 均已消费完成
- **THEN** 该批次 arena 持有的节点内存 SHALL 被回收,不发生泄漏

### Requirement: children 内联小容量存储

`TaskNode` 的 `children` SHALL 使用内联小容量存储(至少 4 槽),当子节点数不超过内联容量时不得触发堆分配。

#### Scenario: 少量子节点无堆分配

- **WHEN** 一个 `TaskNode` 声明依赖 2 个父节点(每个父节点各 1 个子节点)
- **THEN** `children` 存储 SHALL 不使用动态内存分配,子节点指针内联于节点对象中

#### Scenario: 超出内联容量退化

- **WHEN** 一个父节点拥有超过内联容量的子节点
- **THEN** `children` SHALL 退化为堆分配存储,依赖调度行为与内联时一致

### Requirement: 轻量锁保护 children

`TaskNode` SHALL 使用轻量自旋锁(而非 `std::mutex`)保护 `children` 的并发访问,临界区仅覆盖子节点的写入与搬移操作。

#### Scenario: 构建期添加子节点与执行期搬移互斥

- **WHEN** 主线程在 `DependsOn` 中添加子节点,同时工作线程在 `TryEnqueue` 中搬移 `children`
- **THEN** 两者 SHALL 通过自旋锁互斥,不出现数据竞争,子节点不丢失不重复

### Requirement: future 按需分配

`TaskNode` 的 `std::promise` SHALL 仅在调用 `GetFuture()` 时才分配共享状态;未请求 future 的节点不得承担 shared_state 分配开销。

#### Scenario: 不取 future 的节点零 promise 分配

- **WHEN** 创建 `TaskNode` 但从不调用 `GetFuture()`
- **THEN** 该节点 SHALL 不分配 `std::promise` 的 shared_state

#### Scenario: 取 future 仍返回有效句柄

- **WHEN** 调用 `GetFuture()`
- **THEN** SHALL 惰性分配 promise 并返回有效的 `std::future<void>`,任务完成时置位

### Requirement: TaskNode 任务走本地队列

`TaskNode` 依赖图产生的可执行任务 SHALL 被推入 worker 的本地队列(round-robin),由本地队列 + work-stealing + 全局队列三级优先级调度,而非全部汇入全局队列。

#### Scenario: 任务分布到本地队列

- **WHEN** `Submit` 一个多节点的依赖图
- **THEN** 可执行任务 SHALL 被推入各 worker 的 local queue 而非单一 globalQueue

#### Scenario: 依赖图正确完成

- **WHEN** 一个菱形/线性/多根依赖图被 `Submit` 并 `WaitIdle`
- **THEN** 所有节点按依赖顺序各执行一次,无丢失、无重复、无死锁

### Requirement: 调度可调用体瘦身

`TaskNode` 入队时构造的任务可调用体 SHALL 避免额外捕获 `CounterPtr` 自引用带来的引用计数增减,依据 arena 生命周期契约直接持有节点指针。

#### Scenario: 执行期节点指针有效

- **WHEN** 一个 `TaskNode` 进入本地队列等待执行
- **THEN** 其被捕获的裸指针在执行期间 SHALL 保持有效(由 arena 批次生命周期保证)

