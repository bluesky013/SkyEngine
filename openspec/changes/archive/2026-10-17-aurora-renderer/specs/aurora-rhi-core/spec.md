## MODIFIED Requirements

### Requirement: Queue::Submit 提交命令缓冲

Queue SHALL 提供 Submit 协议，将 SubmitInfo 的 commandBuffers 提交至对应队列，并返回可观察的接受结果。结果 SHALL 区分 Accepted、已知未入队的 Rejected，以及 DeviceLost/无法证明是否入队的错误；具体类型命名由实现确定。Accepted SHALL 不表示 GPU 完成。SubmitInfo SHALL 至少支持 commandBuffers、带 stage/value 的 waitSemaphores/signalSemaphores 和可选 fence；空 commandBuffers 的同步提交 SHALL 合法。

后端 SHALL 按各同步原语的作用范围执行 wait/signal；调用方 SHALL 保活 command buffers 及相关资源直到已证明对应 GPU 使用结束。提交错误 SHALL 反馈给调用方，不能仅日志记录；不确定错误不得被调用方当成已知未提交或成功完成。

新增可观察入口 SHALL 在任何 native queue 操作前完成本地参数校验和暂存数组分配，并保留 native 错误码；同类型但属于其它 device 的对象 SHALL 被拒绝。后端尚未实现新协议时 SHALL 明确返回 Unsupported，不得通过调用旧 void Submit 伪造 Accepted。现有 legacy 入口 MAY 保留兼容，但 Renderer 新路径 SHALL 使用结果协议。

#### Scenario: 预检不得部分提交等待
- **WHEN** 输入 wait 合法但后面的 signal 或 command buffer 无效
- **THEN** 整次请求返回 Rejected，不能先把前缀 wait 排入队列而阻塞后续无关提交

#### Scenario: DX12 部分排队后失败
- **WHEN** native Wait/Execute/Signal 已开始，随后调用失败或检测到 device removed
- **THEN** 返回 Indeterminate 及错误，资源继续保活；调用方不能按 Rejected 立即回收

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

#### Scenario: 实际 worker 与编译任务编号不同
- **WHEN** 调度器把同一类编码任务交给不同实际 worker，且任务总数超过 worker 数
- **THEN** 每次从实际 ThreadContext 取得 slot 分区和独立 recording ordinal；任务编号不用于推断 worker，已录结果保持有效到退休

## ADDED Requirements

### Requirement: SwapChain 显式获取与呈现退休

SwapChain SHALL 为 checked 帧提供有界 acquire、可重试 present、未呈现释放、resize/close 和独立的 presentation 资源退休结果，保留具体原生错误。直接宿主协议与 checked 协议 SHALL 互斥。graphics completion SHALL 不单独充当 presentation 退休证明；超时不表示成功或安全释放。

RHI SHALL 提供不 acquire 的显式 surface 查询结果，保留查询错误并区分 surface 有效性、暂停、所选配置需要更新/重建与设备错误。后端 SHALL 根据最新能力与输出策略比较已应用配置，检查 extent、transform 及其它相关创建约束；不得仅检查尺寸或在查询失败时返回旧尺寸作为成功结果。renderer SHALL 不依赖 native caps 结构；更新与销毁仍受在途责任约束。
查询结果 SHALL 包含规范化的 extent/format/transform 和成功创建代次；可用结果表示已应用配置，需要重建的结果表示待应用配置，失败结果 SHALL 不作为准备快照。创建代次 SHALL 不随重复查询增加。

#### Scenario: Vulkan transform 变化但尺寸不变
- **WHEN** 策略选择 preTransform=currentTransform，查询得到新的 currentTransform 且 extent 与当前 swapchain 相同
- **THEN** 后端仍报告配置需要重建，校验所选 transform 的支持性，并向 renderer 提供一致的输出映射；能力枚举顺序变化或不影响所选配置的新增能力不单独触发重建

#### Scenario: surface 查询失败与未指定尺寸
- **WHEN** 原生 surface 查询失败，或 Vulkan currentExtent 为未指定值
- **THEN** 查询失败保留原生错误；未指定尺寸按平台 drawable 像素尺寸和能力范围解析，不能把未指定值当真实 extent，也不能用旧 swapchain 尺寸隐藏变化

#### Scenario: acquire 返回 Suboptimal
- **WHEN** 原生 acquire 成功取得图像但报告 Suboptimal
- **THEN** 保留该图像和同步的完整责任并安排或立即进行配置复核，不丢弃成功结果；相同有效配置不因重复 Suboptimal 每帧无条件重建，OutOfDate 与 SurfaceLost 继续走各自错误路径

#### Scenario: Vulkan 正常呈现与未提交释放
- **WHEN** 使用支持 swapchain-maintenance1 的 Vulkan 设备获取输出
- **THEN** 获取信号、消费提交、presentation fence 和 CPU 借用分别管理；正常复用等待全部责任完成。未提交图像在 acquire fence 确认后显式释放，不等待从未入队的 render-done signal；能力不足返回 Unsupported

#### Scenario: 原生呈现可重试与已排队错误
- **WHEN** Vulkan present 返回 OOM、OutOfDate 或 SurfaceLost，或 DXGI Present 返回 WAS_STILL_DRAWING
- **THEN** 已知未入队的失败保留同帧重试；可能已排队的呈现保留同步和图像责任直到安全退休，错误不被当作完整成功或无限 Pending

#### Scenario: DX12 呈现资源交还
- **WHEN** graphics completion 已确认并成功进行 DO_NOT_WAIT Present
- **THEN** 记录独立的 Present 后 queue fence 和对应 native buffer 所有权条件；它证明应用资源可退休，不宣称屏幕扫描完成。信号失败保留不确定责任，resize/close 不能提前释放仍在途 buffer

#### Scenario: 输出借用跨 lease 或绕过同步
- **WHEN** borrowed output 被编译到另一 DeviceFrameLease，或提交未携带其输出同步所有者
- **THEN** 显式拒绝且不排队；borrowed image 不转换为会删除 swapchain wrapper 的拥有型引用。Accepted/Indeterminate 保留同步 owner 至 GPU 安全收尾，Rejected 保留调用方录制

#### Scenario: 正常关闭与终态退出
- **WHEN** 输出请求 resize/close 或发生无法恢复的提交/退休错误
- **THEN** 正常路径只在该输出 CPU/GPU/presentation 责任清空后执行并返回确认，不等待整个设备；终态隔离不伪报正常确认，销毁保持 native 安全等待和 Device/module/window 寿命

### Requirement: 提交完成查询可区分错误

RHI SHALL 为 scheduler 提供非阻塞或有界的完成查询，区分 Pending、Complete 和 Error/DeviceLost。现有 bool 查询可保留兼容包装，但 renderer SHALL 使用可观察错误的协议，不将永久设备错误解释为无限 Pending。timeout SHALL 只表示尚未确认完成，不能作为资源安全回收证明。

#### Scenario: GPU 在途时设备丢失
- **WHEN** 已接受的 submission 在后续查询期间发生 device lost
- **THEN** 查询返回错误状态，scheduler 不继续无限等待普通 fence，也不把该提交资源当成正常完成后可复用

#### Scenario: fence reset 与提交分别可观察
- **WHEN** idle fence reset 成功后提交被明确拒绝
- **THEN** fence 仍可为 Pending，但不存在可等待的提交；重试或丢弃由 slot 状态决定，不能把 Pending 当作入队证据

#### Scenario: DX12 device removed 哨兵
- **WHEN** native fence GetCompletedValue 返回 UINT64_MAX
- **THEN** 查询报告 DeviceLost，不得按该值大于目标值而报告正常完成

### Requirement: DeviceFrameContext 显式 slot lease

DeviceFrameContext SHALL 提供显式 slot 获取、command/分配访问、提交、未提交丢弃与完成查询协议。lease SHALL 含 slot identity/generation 和 device epoch，提交或丢弃后失效。context SHALL 是 GPU slot 状态、completion 与 native 资源的唯一所有者；首版每个活动 Device SHALL 只有一个 context 和最多一个供 coordinator 建图的 Recording lease。获取 SHALL 非阻塞返回 Ready/Busy/Error，不提前 Reset 提交 fence，不自动推进隐式 frame index。

公开执行方法 SHALL 直接由 DeviceFrameContext 提供，不再通过 GetFrameExecution 或独立 compiler 接口转接。contextId SHALL 跨模块唯一，创建方 SHALL 传入非零 device epoch；不能把复用的 slot index 当成有效 lease。内部 recording ordinal SHALL 有容量上限，失败不得覆盖已录完的命令。直接宿主协议与显式协议 SHALL 在同一 context 内互斥。

#### Scenario: 旧 lease 迟到
- **WHEN** slot 已被新一轮复用或 device epoch 改变，旧 lease 继续访问分配/命令入口
- **THEN** 拒绝该访问，不操作新一轮 slot 的资源

#### Scenario: Recording 无 GPU 工作
- **WHEN** 已取得 lease 但没有任何 GPU 工作且已处理输出获取责任
- **THEN** 可丢弃未提交 recording；不 Reset fence、不制造待完成 submission，下一次 slot 获取不会等待不存在的提交

#### Scenario: 提交接受结果与 slot 状态闭合
- **WHEN** scheduler 请求 context 提交有效 recording
- **THEN** context 接管 GPU refs，在实际提交前 Reset fence，按 Accepted/Rejected/不确定错误推进 Submitted/恢复/隔离分支，不留未登记的 Reset 后失败路径

#### Scenario: 拒绝后重试或丢弃
- **WHEN** fence reset 或 queue submit 明确拒绝，尚未入队
- **THEN** 调用方仍持有 RecordedFrame，可修正后重试；销毁最后一个 recording borrower 后才能 Discard，不查询或等待这个未提交 recording 的 fence

#### Scenario: 完成通知背压
- **WHEN** completion 输出空间为零或不足以交付全部完成项
- **THEN** 未交付 submission 保留引用和 slot；每个 token 最多交付一次，正常完成交付后才可复用 slot

#### Scenario: 不确定提交或完成查询失败
- **WHEN** 无法证明是否入队，或 retained submission 的查询返回 Error/DeviceLost
- **THEN** slot 保留 GPU 引用并隔离，context 停止新 lease；错误事件只交付一次，不能用已报告错误作为提前释放证明，保留到已证明完成或设备安全 teardown

### Requirement: FrameContext allocator 分区一致

command、shader-visible descriptors、RTV/DSV、staging、dynamic UBO 与 batch pack 的 renderer 路径 SHALL 使用同一 DeviceFrameLease 对应的 slot 分区；不同 View SHALL 在其中分配不重叠 slice。后端分区数量和映射 SHALL 与配置的 inflight slots 一致，重配不得破坏在途资源。allocator SHALL 不根据独立 Tick、viewport 或隐式 AdvanceFrame 次数判断复用安全性。

#### Scenario: 非固定轮转与不同输出节拍
- **WHEN** 因空帧、失败或 viewport 暂停而重复使用某个已安全 slot
- **THEN** descriptor/staging/UBO 均选择该 lease 对应分区，不因自身计数器错位覆写其它在途 slot

#### Scenario: 未退休分区容量不足
- **WHEN** 当前 slot 分配超出预算且其它 slot 仍在途
- **THEN** 返回失败/延后或申请受同等退休约束的新 backing，不通过重置其它 slot 腾出空间

#### Scenario: 构建期复制与常量对齐
- **WHEN** coordinator 在有效 Recording lease 上写入 staging 或逐 View UBO 数据
- **THEN** 立即复制输入到该 slot 的独立范围；UBO 按设备对齐并清零 padding，返回的 size 包含 padding，失败不消耗分配游标；返回 buffer 强引用不延长 lease 或授权覆写在途范围

#### Scenario: 编译后的分配冻结
- **WHEN** 成功 Compile 已保留 plan，或 encoding/recording 仍借用 lease
- **THEN** 拒绝新的 buffer/descriptor 分配；worker 只消费提前确定的范围，不能调用 legacy current-frame 分配路径；销毁成功 plan 不自动回卷已返回分配，整槽复用必须重新获取 lease

#### Scenario: native 编译分配失败回滚
- **WHEN** compiler 在分配部分 RTV 或 footprint scratch 后失败
- **THEN** 回滚本次 compiler 内部分配游标，保留编译前的 host slice；同 lease 的合法图可重试，不能覆盖其它在途 slot

#### Scenario: MSAA 能力预检与失败重试
- **WHEN** live 图使用多采样 color 或 D32 depth storage
- **THEN** 各 backend 在 transient 分配前分别查询实际 color/depth format 与 sample 支持，Vulkan 同时按实际 usage 查询；不得用 color 能力替代 depth。不支持返回 Unsupported，原生查询失败保留 cause，同 lease 可重试；被裁剪的资源不查询或分配

#### Scenario: depth resolve 能力与 command 接口预检
- **WHEN** live 图请求 D32 depth resolve
- **THEN** Vulkan 检查实际 supportedDepthResolveModes；DX12 确认 CommandList1 与所需 programmable sample-position tier，不能用 color resolve 或 multisample quality 代替。缺能力在 transient 分配前明确 Unsupported，已有不请求该功能的图仍可用；native 错误保留 cause，同 lease 可重试。worker 不查询能力或 COM 接口

#### Scenario: depth resolve 首次目标 metadata 与退休
- **WHEN** DX12 resolve 首次写入尚未就绪的 depth target cell，或计划取消/提交
- **THEN** 在 DEPTH_WRITE 下只初始化该 cell 的 metadata，再进入 RESOLVE_DEST；已有内容不 discard，metadata 初始化不定义逻辑内容。views、descriptors、command 接口和 backing 沿现有 lease/plan 生命周期，取消先 drain，提交后等 GPU 完成或安全 teardown 才回收；回执必须计入 depth resolve 目标写入

#### Scenario: DX12 resolve 首次目标初始化
- **WHEN** resolve 或局部 copy 首次写入尚未初始化 metadata 的 render-target 子资源
- **THEN** 共享初始化路径先在合法状态初始化该 cell 的 metadata，再转换到实际写状态；不丢弃已有内容或其它 cells，不把 Discard 当作逻辑内容定义，只有真实写入范围获得内容资格

#### Scenario: attachment view 构建与退休
- **WHEN** compiler 为 mip/layer attachment 构建 native image view 或 RTV/DSV
- **THEN** 在 coordinator Compile 完成全部创建和范围解析，worker 仅消费冻结结果；同计划可共享相同 native view，DX12 descriptors 使用当前 lease 分区。失败释放已创建对象且 lease 可重试，取消等待 drain，已提交 views/heaps 保活到 GPU 完成或安全 teardown；framebuffer 先于其引用的 view 销毁

#### Scenario: descriptor 分区 reset 失败
- **WHEN** 已退休 slot 的 native descriptor pool reset 失败
- **THEN** 保留原始错误且不发布新 generation；可恢复失败允许重试，DeviceLost 关闭 context，不将重置失败误报 Ready

#### Scenario: 上传预算查询 lease 剩余容量
- **WHEN** coordinator 在 Compile 前查询 AvailableFrameBufferBytes
- **THEN** 返回对齐及设备 uniform padding 后可接纳的字节数，不分配 native buffer、不推进游标；非法 alignment、错误 context、过期或不可分配 lease 返回显式错误，容量为零不是失败

### Requirement: 后端 FrameContext 编译与 dispatch

DeviceFrameContext SHALL 提供匹配后端的完整 Compile、Dispatch、编码结果收齐和 Submit 协议。Dispatch SHALL 在该 context 管理的 worker pool/ThreadContexts 与 coordinator 编码入口执行后端 EncodeTaskGraph，默认复用 Core TaskDispatcher（RHI 保留 DeviceFrameDispatcher 兼容名）作为 worker 批次执行器，不把 GPU resource DAG 原样当 CPU DAG。coordinator-only 节点 SHALL 回到指定 coordinator，不能依赖现有 dispatcher 尚不提供的线程亲和性。serial 模式 SHALL 使用同一后端计划/结果协议；首版单 graphics queue，SubmissionPlan 可包含多个命令对象。

compiler SHALL 是各后端的私有组合实现；公共 slot 机制只能管理生命周期，不接管 native pass 构建、资源/同步计划或任务划分。尚未支持的阶段 SHALL 显式返回 Unsupported，不能调用旧公共 executor 伪造新协议成功。

#### Scenario: 三后端执行各自计划
- **WHEN** context 收到自身 Compile 生成的合法计划
- **THEN** Vulkan 可组装 secondary/primary，DX12 可收齐并排序 DIRECT lists，Metal 可创建/结束 parallel parent/children；宿主不按 API 分支录制 native passes

#### Scenario: CPU 编码完成而 GPU 未执行
- **WHEN** Dispatch 的全部任务成功并返回 RecordedFrame，但尚未 Submit
- **THEN** 没有 GPU 完成或上传 Resident 通知，没有 fence reset；scheduler 仍可按未提交路径放弃

#### Scenario: 并行路径具有真实重叠
- **WHEN** parallelNum 大于 1 且图含两个可独立录制的 native 单元
- **THEN** 可用受控同步观测不同 worker 的录制重叠；serial 与 parallel 使用各自模式的同一计划语义并得到相同读回结果，不能只依据配置值认定并行完成

#### Scenario: 有界精确 backing 状态纳管
- **WHEN** coordinator 使用 TrackResource 登记一个资源及已知初态（image 可为逐 mip/layer 完整表）
- **THEN** 验证精确 Device 归属、资源/状态类型与容量，重复登记拒绝；持有 backing 防止地址复用，首次 accepted/completed serial 均为零；资源使用只属于该 context/graphics queue，外部使用须先完成所有已纳管使用并移出

#### Scenario: 旧 slot 完成回执不能倒退账本
- **WHEN** 同一 backing 的较新 serial 先被 PollCompleted 报告，较旧 slot 随后报告
- **THEN** completed 状态与 token 不倒退；所有该 backing 的提交回执退休且无 plan/encoding/recording 借用后才允许 UntrackResource，不能让旧回执更新重新登记的条目

#### Scenario: 候选不发布与终态失效
- **WHEN** 编译或编码失败、取消、放弃、Rejected、Indeterminate 或完成查询错误
- **THEN** 未接受候选不推进 accepted/completed；Rejected 可保留同录制重试；不确定提交和终态错误使整个 context 账本不可查询或重用，保留 pins 至安全 teardown

#### Scenario: 有 owner 的账本租约自动退休
- **WHEN** TrackOwnedResource 的最后一个共享 owner 被释放
- **THEN** owner 析构不调用 context/GPU；下一次协调线程的纳管、取槽、丢弃或完成轮询入口，只在所有 plan/encoding/recording 借用和已提交使用退出后回收该条目。活跃 owner 不能经 Untrack 强制移出，失败隔离不被自动回收绕过

#### Scenario: 账本租约晚于 context 释放
- **WHEN** context 已安全退出而调用方仍保留 tracking owner，且 Device/backend module 继续存活
- **THEN** owner 不引用已销毁 context，后续释放不访问悬空 context；资源状态不能被转交给另一 context 冒充同一队列状态

#### Scenario: 精确 image cell 容量与退休
- **WHEN** coordinator 纳管 image 或安全移出已纳管 image
- **THEN** 从验证归属的 native backing 获取真实 mip/layer cell 数；未知 storage 或非法状态表拒绝。账本分别限制 backing 数与 image cell 总数，默认 1024/65,536；满额不发布条目，manual/owned 安全退休归还计数，终态不提前回收

#### Scenario: 单 cell 过期初态与在途接续
- **WHEN** 后图声明的 image initial 仅一个 mip/layer 与 accepted 状态不符
- **THEN** 整图 Compile 拒绝且 lease 可重试；从 accepted 完整快照构建的后图可在前图完成前有序 Accepted。编译/放弃不发布候选，completed 仍按完整 SubmissionToken 的 serial 单调更新

### Requirement: EncodingSession drain 与分配隔离

EncodingSession SHALL 持有 plan、任务结果、CPU borrowers 与候选 GPU leases，只有全部任务/continuations 退出且 native 编码对象收尾后才可完成或丢弃。任务 SHALL 通过显式结果报告失败并阻止依赖任务正常执行，不使用 throw/try/catch；已启动任务仍须收齐。节点预算、空图、单 worker、大 fan-out SHALL 有进展保证；worker SHALL 不阻塞等待依赖子任务或 coordinator。worker scratch/结果写入 SHALL 独立，descriptor/cache 写入 SHALL 在并行消费前冻结或有显式同步。

#### Scenario: 编译中途 descriptor 分配失败
- **WHEN** Compile 已分配部分 lease descriptor 后遇到容量或原生错误，且没有产生成功 plan
- **THEN** 回收本次未提交分配并恢复预算，不释放该 lease 此前已有的分配；清理失败显式保留原生原因和不能安全复用的占用，DeviceLost 不被次要清理错误掩盖

#### Scenario: 取消时另一个 worker 仍录制
- **WHEN** 一个编码任务失败或收到取消，另一个任务仍访问 slot
- **THEN** 不重置 allocator/arena、不复用 slot；drain 后返回失败/取消且不允许 Submit 部分结果

#### Scenario: 编码预算拒绝与重试
- **WHEN** 后端计划的分片数超过 recording 或 dispatcher 队列预算
- **THEN** 在启动 worker 前显式失败，不产出部分 RecordedFrame；正确 ticket 收齐后可释放未提交 lease，后续合法帧仍可编码

#### Scenario: 失败先于取消但其它任务未退出
- **WHEN** 某个 worker 返回 native 编码错误，另一个 worker 仍运行，随后 coordinator 请求 Cancel
- **THEN** TryCollect 在 drain 前保持 Pending，不能 reset/discard slot；收齐后保留原生错误并返回 Failed，不将已发生的失败改写为 Cancelled

#### Scenario: 多个 worker 的错误包含设备丢失
- **WHEN** 较早编号的片段报告可恢复错误，另一个片段报告 DeviceLost，且随后收到取消
- **THEN** 收齐所有工作后返回包含原生设备丢失原因的 Failed，关闭 context，禁止再取得 slot；不能仅选择最早编号错误而继续复用

#### Scenario: teardown 期间仍有 CPU 与 GPU 借用
- **WHEN** 销毁 FrameContext 时仍有编码任务或未退休提交，且调用方保持 Device/backend module 存活
- **THEN** 请求取消未启动工作并等待已启动工作退出，保留提交资源直到安全 GPU 收尾，再销毁 worker/slot pools；不能只等 CPU 编码结束便释放 GPU 使用的命令资源

#### Scenario: DX12 native list 失败后的复用
- **WHEN** DIRECT list 的 Reset 或 Close 返回失败，编码批次已收齐且未提交
- **THEN** 返回包含 HRESULT 的 Failed，不提交其它已录片段；不可复用的 native list 被丢弃，后续安全 lease 重建，仍在使用的 allocator 不提前 reset

#### Scenario: coordinator 续接 parent encoder
- **WHEN** worker 子批次完成且后续节点必须由 coordinator 结束 parent 或组装 primary
- **THEN** 通过完成通知唤醒 coordinator 执行该节点，不在 worker 假装具有 coordinator 身份；所有节点完成前不返回 RecordedFrame


### Requirement: 新帧协议的平台能力门

FrameContext 的 lease、Compile/Dispatch、受检提交与输出退休 SHALL 作为整套能力验证。已迁移 Windows Vulkan/DX12 SHALL 使用该协议；尚未实现的 Metal 或未验收宿主 SHALL 明确拒绝新入口或保留完整旧模式，不能借用旧 void 操作伪造新协议成功。新协议的后端细则仅在声明支持后适用；后续接入 SHALL 满足相同内容、失败与生命周期语义。

#### Scenario: 未实现的新后端入口
- **WHEN** 宿主请求尚未支持新帧协议的 Metal Renderer
- **THEN** 返回 Unsupported，不能创建一个表面成功但不执行的 Renderer

#### Scenario: 保留旧宿主模式
- **WHEN** 未迁移平台启动既有宿主
- **THEN** 在初始化前选择完整旧流程，不与新 Renderer 共用 Device 再建立第二套 slot 或提交循环
