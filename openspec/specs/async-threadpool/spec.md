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

### Requirement: 遗留 taskflow 作业系统迁移到 ThreadPool

`Task` / `TaskExecutor` / `NamedThread` SHALL 基于引擎自研 `ThreadPool`（+ `TaskNode`）实现，core SHALL NOT 依赖 `taskflow`。`Task` 的依赖句柄 SHALL 为 `TaskNodePtr`（别名 `TaskHandle`），`StartAsync` SHALL 经 `CreateTask` + `DependsOn` + `Submit` 表达依赖图；`TaskExecutor::WaitForAll` SHALL 委托 `ThreadPool::WaitIdle`。

#### Scenario: Task 依赖经 ThreadPool 表达

- **WHEN** 一个 `Task` 声明对其它 `Task` 的依赖并 `StartAsync`
- **THEN** 其依赖句柄为 `TaskNodePtr`，依赖边经 `TaskNode::DependsOn` 建立，父任务完成后本任务才执行

#### Scenario: core 源文件不依赖 taskflow

- **WHEN** 编译 core 的 `Task` / `NamedThread`
- **THEN** 其头/源不 `#include <taskflow/...>`，实现基于 `ThreadPool`（taskflow 依赖在 legacy shader/render 迁移完成前仍由 core 目标传递链接保留）

#### Scenario: NamedThread 基于单线程池

- **WHEN** 构造 `NamedThread` 并 `Dispatch`/`Sync`/`Signal`
- **THEN** 行为与迁移前一致（`Sync` 等待该线程执行到 `Signal`），实现基于 `ThreadPool(1)`

### Requirement: 通用有界批次 dispatcher

Core SHALL 提供不依赖 Renderer/RHI 的 TaskDispatcher，复用原 DeviceFrameDispatcher 的静态 DAG 执行机制；RHI 原头 SHALL 保留兼容类型名。图构建、提交和回收 SHALL 属于同一 owner，运行中不得修改或清空图。批次 SHALL 使用独占 ThreadPool，节点总数不得超过 workerCount 乘本地队列容量；worker SHALL 不等待同池任务。round-robin SHALL 在 worker 数不是二次幂时仍保持均匀轮转。

#### Scenario: 无效图在入队前拒绝
- **WHEN** 计划包含环、非法边、空 callback 或节点总数超过预算
- **THEN** Submit 返回明确错误且不执行任何节点；调用方可 Clear 后重建

#### Scenario: 空批次和重复使用
- **WHEN** 提交空图，或前一批次完成后 Clear 并重新提交
- **THEN** 空图立即成功，新批次只执行自己的节点；运行中 Clear 和未 Clear 的重复提交明确拒绝

### Requirement: 批次显式失败与完成收齐

TaskDispatcher SHALL 通过返回值报告建图/提交错误，以带状态的 future 报告节点和批次结果，不抛异常、不捕获异常、不将异常存入 future。callback SHALL 遵守不抛异常契约；可能失败的任务 SHALL 使用 checked callback。checked callback 返回失败时 SHALL 将节点记为 Failed，传递消费者记为 Skipped，独立节点继续。批次完成 SHALL 在所有节点退出且结果可见后发布；内部 drain hook SHALL 与 promise 具有独立所有权，允许 hook 回收已收齐的图存储；join future SHALL 在 hook 返回后就绪。内部 hook 不得等待该批次或直接执行用户 continuation。之后回收图存储不得与完成回调访问冲突。标准库异常当前不纳入错误协议。

#### Scenario: 失败节点和独立节点
- **WHEN** 一个 checked 节点返回失败，而图中另有独立工作
- **THEN** 依赖消费者不调用 callback，独立工作执行完成，节点与批次 future 返回相应状态

#### Scenario: 重复获取节点 future
- **WHEN** 调用方第二次请求同一节点的 future
- **THEN** 返回无效 future 并记录 INVALID_STATE；Submit 在执行任何 callback 前拒绝该批次

### Requirement: 通用 Completion 与指定 executor 交付

Core SHALL 提供不依赖 Renderer/RHI、与协程语法无关的 Completion 协议，区分 Pending、Succeeded、Failed 和 Cancelled；结果发布后最终状态 SHALL 只确定一次。CancelRequested SHALL 是独立请求，不自动完成任务或证明业务资源已退休。调用方 SHALL 可非阻塞查询或订阅结果；订阅与完成竞态 SHALL 不丢通知、不重复交付。业务 JobGroup SHALL 自行定义何时满足完成条件，不能由通用 Completion 推断 GPU 或 ECS 生命周期。

订阅 SHALL 指定 executor，callback SHALL 排队交付而非在发布者栈上隐式执行。Subscribe 返回 Busy/Rejected 时 SHALL 保留调用方 callback 所有权；Accepted SHALL 预留交付容量并保活 executor、callback 及其模块至交付结束。普通 worker 队列满或 shutdown SHALL 不得丢弃已接受的通知；关闭 SHALL 停止接收并排空已接受订阅，可按任务所属协议请求取消，但最终仍交付唯一完成状态，不将已发布成功改成订阅者各异的取消。错误 SHALL 使用显式返回值，不使用 throw/try/catch；标准库异常不纳入该协议。

executor SHALL 由目的线程 pump；错误线程与嵌套 pump 返回显式错误。容量 SHALL 包括 Pending 订阅、Ready 通知和正在执行的 callback，直到捕获对象析构才归还；关闭后 owner SHALL 继续驱动已接受通知到 drained。生产者 SHALL 保持 source 并在任务收齐后发布，放弃 token 不改变这一义务。模块加载方 SHALL 向 executor keepAlive 或 callback captures 提供代码租约。

#### Scenario: 完成前后订阅与发布竞争
- **WHEN** 两个订阅分别在完成前后注册，或注册与完成同时发生
- **THEN** 每个已接受订阅在指定 executor 恰好执行一次并看到同一最终结果，不在发布线程意外重入

#### Scenario: 交付容量不足
- **WHEN** executor 无法为新订阅预留交付容量
- **THEN** Subscribe 返回 Busy 且不消费 callback；已接受订阅不受后续队列满影响

#### Scenario: 回调退出前容量仍占用
- **WHEN** callback 在执行中尝试嵌套 pump 或新增超容量订阅
- **THEN** 嵌套 pump 返回明确错误，订阅返回 Busy 且保留 callback；捕获对象和 executor pin 不提前退休

#### Scenario: 取消与关闭不提前释放
- **WHEN** CancelRequested 或 executor shutdown 与任务完成竞争
- **THEN** 最终结果只发布一次，已接受通知及其捕获对象和模块保持有效到交付退出；取消请求本身不代表底层工作结束

