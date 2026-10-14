## MODIFIED Requirements

### Requirement: Queue::Submit 提交命令缓冲

Queue SHALL 提供 Submit 协议，将 SubmitInfo 的 commandBuffers 提交至对应队列，并返回可观察的接受结果。结果 SHALL 区分 Accepted、已知未入队的 Rejected，以及 DeviceLost/无法证明是否入队的错误；具体类型命名由实现确定。Accepted SHALL 不表示 GPU 完成。SubmitInfo SHALL 至少支持 commandBuffers、带 stage/value 的 waitSemaphores/signalSemaphores 和可选 fence；空 commandBuffers 的同步提交 SHALL 合法。

后端 SHALL 按各同步原语的作用范围执行 wait/signal；调用方 SHALL 保活 command buffers 及相关资源直到已证明对应 GPU 使用结束。提交错误 SHALL 反馈给调用方，不能仅日志记录；不确定错误不得被调用方当成已知未提交或成功完成。

#### Scenario: 单 cmdbuf 提交无信号无 fence
- **WHEN** 调用方提交一个有效 command buffer 并取得 Accepted
- **THEN** 之后成功的 Queue::WaitIdle 可证明该队列工作完成；Submit 返回本身不能证明完成

#### Scenario: Submit 携带 fence 完成查询
- **WHEN** 有效 Submit 携带已 reset 的 fence 且被 Accepted
- **THEN** fence 在对应 GPU 工作完成时 signal；立即查询可能 Pending 或 Complete，不保证 GPU 一定尚未完成

#### Scenario: Submit signal binary semaphore 给后续 submit wait
- **WHEN** 第一次有效提交 signal binary semaphore S，第二次有效提交 wait S
- **THEN** 后续消费者在所声明同步范围满足之前不执行相应访问，且 binary semaphore 只按合法生命周期复用

#### Scenario: Submit 支持空 cmdbuf 列表用于纯信号传递
- **WHEN** commandBuffers 为空且有合法 wait/signal/fence 配置
- **THEN** 可接受该同步提交，满足等待后完成对应信号

#### Scenario: fence reset 后提交被拒绝
- **WHEN** 底层已知没有接受该提交
- **THEN** 返回 Rejected 及错误信息，上层不把 slot 留在等待该 fence 的正常 Submitted 状态

#### Scenario: 提交遇到 device lost 或不确定错误
- **WHEN** 后端不能证明提交未入队或设备已丢失
- **THEN** 反馈对应错误，上层隔离潜在在途资源并停止正常复用，走设备错误退休协议

### Requirement: FrameContext 按 inflightNum 预分配 command buffer

每个后端 DeviceFrameContext SHALL 持有由 inflightNum 决定的唯一 GPU slot ring，每个 slot 预留 coordinator 主编码资源；可复用后端 primary buffer/list 包装，但不得强迫所有 native API 使用 Vulkan primary/secondary 模型。RDG 路径 SHALL 在 context 内通过有效 DeviceFrameLease 使用资源，宿主不另建 command ring。command pool/allocator 的 reset 域 SHALL 与退休域一致，不得重置其它 slot 仍在途或同 slot 已录完待提交的命令资源。

#### Scenario: inflightNum 控制 buffer 数量
- **WHEN** 调用 CreateFrameContext 且 inflightNum 为 3
- **THEN** context 持有 3 份 coordinator 主编码资源，并在各自有效 lease 下使用，宿主无需另建 command ring

#### Scenario: 默认 inflightNum 为 2
- **WHEN** 使用默认初始化参数
- **THEN** 持有 2 份独立的 slot 主编码资源

#### Scenario: 一个 slot 完成而另一个仍在途
- **WHEN** 重用已完成 slot 的 command 资源
- **THEN** 不 reset 包含其它 pending command buffer 的共享 pool，也不 reset 仍被执行的 DX12 command allocator

### Requirement: FrameContext 按 parallelNum 预分配并行编码 command buffer

DeviceFrameContextInitInfo.parallelNum SHALL 只控制 GPU 编码 worker；默认 1 时不创建额外 worker pool，使用后端串行 dispatch；大于 1 时按 worker × inflightNum 预备独立 recording arena/context。Vulkan/DX12 MAY 预分配一份 buffer/list 作为每分区起始容量，任务数大于 worker 数时 SHALL 保留多个 recording ordinal 的结果或合法合并任务，不能 reset 上一个待提交结果。Metal SHALL 可使用 parent 分配的 child encoder，不强制固定数量 secondary buffers。reset 域 SHALL 隔离其它 worker/slot 的录制和 GPU 使用。

Vulkan SHALL 使用 worker 所属 command pool 的安全 slot 分区；DX12 SHALL 独占 DIRECT command list/allocator；Metal SHALL 保留 worker autorelease context 和显式持有的 command 资源。任务 SHALL 从实际执行 ThreadContext 取得资源，除非调度器明确提供亲和性，不得假定任务索引等于 worker 索引。worker 数 SHALL 不被解释为 CPU ECS prepare 的并行度。

#### Scenario: parallelNum > 1 时准备 recording 分区
- **WHEN** inflightNum 为 3 且 parallelNum 为 2
- **THEN** context 有 3 个主编码分区和 6 个 worker/slot recording 分区；native buffers/child encoders 根据后端计划及预算供给

#### Scenario: 默认 parallelNum 为 1 不额外分配
- **WHEN** 使用默认 parallelNum
- **THEN** 不创建编码 worker pool；仍走本后端计划的串行 dispatch，renderer 可另用专用 CPU pool 并行 prepare

#### Scenario: 单 worker 编码多个任务
- **WHEN** 同一 worker 在一个 slot 连续编码多个 native 单元
- **THEN** 各单元结果保持有效到提交/退休，不覆盖前一个结果，不并发使用同一 allocator 或普通 encoder

## ADDED Requirements

### Requirement: 提交完成查询可区分错误

RHI SHALL 为 scheduler 提供非阻塞或有界的完成查询，区分 Pending、Complete 和 Error/DeviceLost。现有 bool 查询可保留兼容包装，但 renderer SHALL 使用可观察错误的协议，不将永久设备错误解释为无限 Pending。timeout SHALL 只表示尚未确认完成，不能作为资源安全回收证明。

#### Scenario: GPU 在途时设备丢失
- **WHEN** 已接受的 submission 在后续查询期间发生 device lost
- **THEN** 查询返回错误状态，scheduler 不继续无限等待普通 fence，也不把该提交资源当成正常完成后可复用

### Requirement: DeviceFrameContext 显式 slot lease

DeviceFrameContext SHALL 提供显式 slot 获取、command/分配访问、提交、未提交丢弃与完成查询协议。lease SHALL 含 slot identity/generation 和 device epoch，提交或丢弃后失效。context SHALL 是 GPU slot 状态、completion 与 native 资源的唯一所有者；首版每个活动 Device SHALL 只有一个 context 和最多一个供 coordinator 建图的 Recording lease。获取 SHALL 非阻塞返回 Ready/Busy/Error，不提前 Reset 提交 fence，不自动推进隐式 frame index。

#### Scenario: 旧 lease 迟到
- **WHEN** slot 已被新一轮复用或 device epoch 改变，旧 lease 继续访问分配/命令入口
- **THEN** 拒绝该访问，不操作新一轮 slot 的资源

#### Scenario: Recording 无 GPU 工作
- **WHEN** 已取得 lease 但没有任何 GPU 工作且已处理输出获取责任
- **THEN** 可丢弃未提交 recording；不 Reset fence、不制造待完成 submission，下一次 slot 获取不会等待不存在的提交

#### Scenario: 提交接受结果与 slot 状态闭合
- **WHEN** scheduler 请求 context 提交有效 recording
- **THEN** context 接管 GPU refs，在实际提交前 Reset fence，按 Accepted/Rejected/不确定错误推进 Submitted/恢复/隔离分支，不留未登记的 Reset 后失败路径

### Requirement: FrameContext allocator 分区一致

command、shader-visible descriptors、RTV/DSV、staging、dynamic UBO 与 batch pack 的 renderer 路径 SHALL 使用同一 DeviceFrameLease 对应的 slot 分区；不同 View SHALL 在其中分配不重叠 slice。后端分区数量和映射 SHALL 与配置的 inflight slots 一致，重配不得破坏在途资源。allocator SHALL 不根据独立 Tick、viewport 或隐式 AdvanceFrame 次数判断复用安全性。

#### Scenario: 非固定轮转与不同输出节拍
- **WHEN** 因空帧、失败或 viewport 暂停而重复使用某个已安全 slot
- **THEN** descriptor/staging/UBO 均选择该 lease 对应分区，不因自身计数器错位覆写其它在途 slot

#### Scenario: 未退休分区容量不足
- **WHEN** 当前 slot 分配超出预算且其它 slot 仍在途
- **THEN** 返回失败/延后或申请受同等退休约束的新 backing，不通过重置其它 slot 腾出空间

### Requirement: 后端 FrameContext 编译与 dispatch

DeviceFrameContext SHALL 提供匹配后端的完整 Compile、Dispatch、编码结果收齐和 Submit 协议。Dispatch SHALL 在该 context 管理的 worker pool/ThreadContexts 与 coordinator 编码入口执行后端 EncodeTaskGraph，默认复用 DeviceFrameDispatcher 作为 worker 批次执行器，不把 GPU resource DAG 原样当 CPU DAG。coordinator-only 节点 SHALL 回到指定 coordinator，不能依赖现有 dispatcher 尚不提供的线程亲和性。serial 模式 SHALL 使用同一后端计划/结果协议；首版单 graphics queue，SubmissionPlan 可包含多个命令对象。

#### Scenario: 三后端执行各自计划
- **WHEN** context 收到自身 Compile 生成的合法计划
- **THEN** Vulkan 可组装 secondary/primary，DX12 可收齐并排序 DIRECT lists，Metal 可创建/结束 parallel parent/children；宿主不按 API 分支录制 native passes

#### Scenario: CPU 编码完成而 GPU 未执行
- **WHEN** Dispatch 的全部任务成功并返回 RecordedFrame，但尚未 Submit
- **THEN** 没有 GPU 完成或上传 Resident 通知，没有 fence reset；scheduler 仍可按未提交路径放弃

### Requirement: EncodingSession drain 与分配隔离

EncodingSession SHALL 持有 plan、任务结果、CPU borrowers 与候选 GPU leases，只有全部任务/continuations 退出且 native 编码对象收尾后才可完成或丢弃。任务失败 SHALL 被捕获并阻止依赖任务正常执行，已启动任务仍须收齐。节点预算、空图、单 worker、大 fan-out SHALL 有进展保证；worker SHALL 不阻塞等待依赖子任务或 coordinator。worker scratch/结果写入 SHALL 独立，descriptor/cache 写入 SHALL 在并行消费前冻结或有显式同步。

#### Scenario: 取消时另一个 worker 仍录制
- **WHEN** 一个编码任务失败或收到取消，另一个任务仍访问 slot
- **THEN** 不重置 allocator/arena、不复用 slot；drain 后返回失败/取消且不允许 Submit 部分结果

#### Scenario: coordinator 续接 parent encoder
- **WHEN** worker 子批次完成且后续节点必须由 coordinator 结束 parent 或组装 primary
- **THEN** 通过完成通知唤醒 coordinator 执行该节点，不在 worker 假装具有 coordinator 身份；所有节点完成前不返回 RecordedFrame
