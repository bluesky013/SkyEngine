## ADDED Requirements

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
