## ADDED Requirements

### Requirement: Renderer 对象与模块边界

系统 SHALL 通过 engine 侧 `IRenderer` 接口提供渲染能力；新增实现 SHALL 位于 provider plugin 的 adaptor target。接口 SHALL 只引用句柄、描述、命令和结果数据；不得暴露 World/Actor、具体 plugin 或可变渲染 ECS。活动宿主 SHALL 共享一个 Renderer/Device，GPU 资源缓存 SHALL 按 device 共享。

宿主 SHALL 通过 `IRendererProvider` 创建对象，创建结果 SHALL 为明确错误或非空、已初始化的 Renderer；未实现的运行时或不支持的新后端帧协议不得伪报成功。成功对象 SHALL 提供已解析 backend 和本实例稳定的非零 deviceEpoch；首版 device loss SHALL 进入失败/关闭流程，新实例 SHALL 使用新 epoch。公共接口消费端 SHALL 不链接 provider 实现或 RHI.Static；provider module SHALL 通过共享 Environment 发布服务，不能覆盖另一活动 provider 的注册。

#### Scenario: 游戏与编辑器使用同一接口
- **WHEN** 分别启动游戏宿主或编辑器宿主
- **THEN** 各自通过接口解析 Renderer provider，且每个进程只创建一个活动渲染设备；宿主不链接 provider 的具体实现

#### Scenario: 接口与模块注册独立验证
- **WHEN** 消费端仅链接接口 target，或通过动态/静态模块入口注册并卸载 provider
- **THEN** 公共头可独立编译链接；宿主可解析同一服务，重复注册不覆盖原 owner，卸载只清除自身注册且允许后续重新加载

### Requirement: Scene View Viewport 独立生命周期

Renderer SHALL 独立管理 Scene、View 和 Viewport。Scene SHALL 保存 ECS 场景状态；View SHALL 引用一个 Scene，保存独立相机、过滤、可见集合、LOD 和历史状态；Viewport SHALL 描述原生窗口或离屏输出。首版每个场景 View SHALL 关联一个输出，原生窗口 SHALL 可合成多个离屏 View。Scene、Entity、View、Viewport IDs SHALL 包含 generation，销毁确认前不得复用槽位，generation 耗尽时 SHALL 退休槽位。

#### Scenario: 同场景双相机
- **WHEN** 两个 View 引用同一 Scene 并使用不同相机和输出尺寸
- **THEN** mesh/material GPU 资源共享，两套相机、裁剪、LOD、历史与输出尺寸相互独立

#### Scenario: Scene 销毁与迟到更新
- **WHEN** Scene 被销毁且旧 generation 的实体或 View 更新随后到达
- **THEN** 更新被拒绝，相关 View 停止场景绘制并报告状态，原生窗口不被隐式销毁

### Requirement: FramePacket 线程与所有权协议

World/UI SHALL 留在宿主主线程，渲染 ECS、Device 与 GPU caches SHALL 由专用渲染线程持有和协调。CPU prepare worker SHALL 仅通过已声明的受限访问借用 ECS/快照；结构变更、GPU 对象/cache 修改、逻辑建图与队列提交 SHALL 由 coordinator 控制。GPU 命令编码 SHALL 由匹配后端的 FrameContext 按计划在 coordinator/encode workers 执行，使用独占编码资源与已发布绑定，不访问 live ECS。主线程 writer SHALL 提交拥有数据的 FramePacket；资产 worker 结果 SHALL 先在主线程归并。包 SHALL 携带顺序号及有序生命周期/更新命令，按 FIFO 处理。载荷 SHALL 按值持有或明确转移不可变所有权；CPU 源数据 SHALL 保活到全部读取/完整 staging 拷贝完成，staging SHALL 保活到 GPU 上传完成。CPU 待处理包上限 SHALL 与 GPU inflight slot 数分开配置。

#### Scenario: 队列已满
- **WHEN** 主线程 TrySubmit 遇到有界队列满
- **THEN** 返回 Busy 且原包仍归调用方；宿主继续处理平台事件后重试，不静默丢弃 create/destroy 命令

#### Scenario: 提交所有权可观察
- **WHEN** TrySubmit 返回 Accepted、Busy 或 Rejected
- **THEN** Accepted 消费并清空调用方包；Busy/Rejected 保留包不变，Accepted 本身不证明 CPU 命令、GPU 或 presentation 已完成

#### Scenario: 异步资产过期
- **WHEN** 资产结果的 entity generation 或 asset revision 与当前状态不符
- **THEN** 渲染线程不应用该结果，并安全回收其载荷

### Requirement: 每帧场景更新与视图建图

Renderer SHALL 每帧对每个 Scene 串行应用命令，冻结结构后执行一次 CPU prepare DAG；依赖不冲突的 systems SHALL 可并行。该 Scene 的所有 prepare jobs 完成后 SHALL 发布 SceneReady，再分别求值各有效 View；View SHALL 只读已准备的 Scene 并写自身临时数据。coordinator SHALL 在所需 CPU jobs 完成后串行建立一张 Frame RDG，包含场景渲染、离屏消费、UI 合成与输出；由匹配的后端完整编译并在其 DeviceFrameContext 中串行或并行编码，首版使用单 graphics queue 提交。RDG handles SHALL 仅在当帧图中使用。

#### Scenario: 离屏结果参与主窗 UI
- **WHEN** 两个场景 View 的离屏结果在同帧被主窗 UI 采样
- **THEN** 同一 RDG 显式包含场景写入到 UI 读取的依赖，且共享 Scene 的准备只执行一次

#### Scenario: Scene prepare 与多 View 并行
- **WHEN** Bounds 准备和 Light 准备仅共享只读 WorldInfo，之后两个 View 读取同一 Scene
- **THEN** 两个 prepare system 可同时就绪，View jobs 等待 SceneReady 后可并行，各自写入独立 visibility/LOD/draw 数据

### Requirement: Prepare 分块与确定性归并

ScenePrepareScheduler SHALL 支持 system DAG 并行，以及显式 element-wise system 的 entity-range 分块。每个 chunk SHALL 只写自己的实体范围；该 system 的整个组件访问范围 SHALL 保留至所有 chunks 完成，下游不得提前执行。共享 append/reduce SHALL 使用任务私有结果和稳定顺序归并。worker SHALL 不修改 RDG builder、GPU cache 或共享 FrameAllocator 游标。结构命令 SHALL 私有记录后确定性归并，默认在下一帧 ApplyCommands 生效。

#### Scenario: 单 system 多 chunk
- **WHEN** WorldBounds system 被切为多个不重叠实体区间
- **THEN** 各区间可并行写已有 Bounds，消费者等待全部区间完成；chunk 不得借任意实体查询写入其它区间

#### Scenario: 稳定输出与延迟结构命令
- **WHEN** worker 完成顺序变化，且某 chunk 请求创建或移除组件
- **THEN** 当前帧池地址和查询集合保持不变；输出与命令按固定 system/chunk 顺序归并，命令在下一帧边界应用

### Requirement: Prepare 执行器完成与失败协议

CPU prepare SHALL 复用现有 ThreadPool 实现，通过每帧 JobGroup 跟踪完成。首版 SHALL 使用 Renderer 持有的专用 prepare pool，所有 Scene/View 共用，并限制整个活动计划节点数以满足该池队列的进展预算。worker SHALL 不阻塞等待同池子任务；依赖 SHALL 在提交前建完。空计划 SHALL 立即完成。CPU 下一帧的 live ECS 修改、Scene 销毁、CPU arena reset 和 module 卸载 SHALL 等待引用它们的 jobs 全部退出。任务失败 SHALL 被捕获并正常释放完成依赖，下游跳过无效结果；失败 Scene SHALL 暂停并报告，恢复需重建，不发布半完成状态。

#### Scenario: 空计划与单 worker
- **WHEN** Scene 没有可处理实体，或用单 worker 执行带依赖及大量逻辑 chunks 的计划
- **THEN** 空计划直接完成；非空计划在预算内批处理并完成，不因零 chunk future、队列饱和或嵌套等待停滞

#### Scenario: system DAG 超过节点预算
- **WHEN** 合法计划的 system 节点数本身超过可安全提交的预算
- **THEN** 使用相同依赖顺序的串行执行回退并完成，不向有界队列无上限提交

#### Scenario: 任务失败时关闭 Scene
- **WHEN** 某 prepare job 失败且宿主同时请求关闭该 Scene
- **THEN** 所有已启动 jobs 完成或跳过后才回收其内存和回调，Scene 不继续发布部分 prepare 结果

### Requirement: Prepare 逻辑完成与恢复上下文

Prepare JobGroup SHALL 提供与协程语法无关的 Pending/Succeeded/Failed/Cancelled 状态与完成令牌，结果发布后只完成一次；coordinator SHALL 能检查状态或订阅 continuation，不依赖阻塞 future.get 判断逻辑完成。CancelRequested SHALL 是独立请求标志；全部已启动 kernels 退出、未启动节点被收齐后才能发布最终完成/取消结果。CPU consumers/continuations 的借用 SHALL 独立保活到退出。订阅与完成竞态 SHALL 不丢通知、不重复运行；continuation SHALL 排队交付到显式指定的 executor。取消 SHALL 不被当作底层 jobs/GPU 工作已退休的证明。首版 prepare kernel SHALL 一次执行至完成，不挂起，不持有 ECS 访问或任务临时存储跨异步等待；尚未完成的 coroutine SHALL 不被当作普通 TaskNode 已完成的结果。

#### Scenario: worker 完成后恢复协调逻辑
- **WHEN** 最后一个 worker 完成 JobGroup，而后续步骤需要访问 GPU cache 或构建 RDG
- **THEN** 结果只发布一次，后续步骤排队回 render coordinator，不能在该 worker 上直接执行

#### Scenario: 订阅已完成任务或同时取消
- **WHEN** continuation 注册时任务已完成，或取消与完成通知并发
- **THEN** 按完成状态协议恰好交付一次结果或取消结果；捕获对象在相关 jobs 与通知退出前保活，不重复恢复或提前释放

#### Scenario: continuation 交付预算耗尽
- **WHEN** 指定 executor 的订阅交付容量不足
- **THEN** Subscribe 返回 Busy 并保留调用方 callback；Accepted 必须已保留交付容量和 executor/module 生命周期，之后不能因普通 worker 队列满而丢弃通知

#### Scenario: 本帧资源尚未就绪
- **WHEN** 某网格还在 IO、shader 编译或独立资源准备中
- **THEN** 本帧使用声明的跳过/占位策略，不在持有 ECS 访问时等待该外部异步流程；完成结果在后续命令边界校验版本后应用

#### Scenario: 取消时仍有 chunk 和结果消费者
- **WHEN** CancelRequested 到达时仍有 chunk 写入，之后仍有 consumer 使用其结果
- **THEN** 最终 JobGroup 结果等待所有 kernels 退出；ECS 下一轮写入或 arena 回收还等待相关 consumer 退出，提前取消回执不替代这两个条件

### Requirement: FrameSlot 提交与退休

FrameScheduler SHALL 区分 packet sequence、submission serial、frame slot 和 swapchain image index，并使用 DeviceFrameContext 的显式 lease。DeviceFrameContext SHALL 唯一持有 GPU FrameSlot 状态和资源，跟踪 Available、Recording、Submitted、Retired 以及明确的错误分支，并保活 command buffer、UBO、descriptor、上传载荷和 GPU 资源引用直到相应提交完成。scheduler SHALL 持业务 SubmissionRecord，不另建 GPU ring。只有确定提交时 SHALL Reset 提交 fence；空帧 SHALL 返回可用状态。提交失败 SHALL 进入明确的错误/恢复状态并报告，不得等待永远无法完成的 fence。

#### Scenario: 全部窗口最小化后恢复
- **WHEN** 所有窗口输出不可用且无离屏 GPU 工作，随后任一窗口恢复
- **THEN** 期间仍处理生命周期命令，不产生未提交的未 signal fence，恢复后可继续提交

#### Scenario: 多帧在途
- **WHEN** GPU 尚未完成某个 slot 而 CPU 处理后续 packet
- **THEN** 未完成 slot 的常量、descriptor、上传区与资源引用不被覆写或释放

### Requirement: CPU 准备与 FrameContext 编码资源分离

CpuPrepareFrame SHALL 在未取得 GPU slot 时即可持有 CPU 输入、JobGroup 与 task/chunk scratch；资源释放 SHALL 等待对应 jobs/borrowers。FrameBuildContext SHALL 组合 PreparedFrame、DeviceFrameLease、OutputLeases 与单线程 GraphBuildArena。Graph/compiled 非平凡对象和录制回调 SHALL 在 arena reset 前析构或结束，GPU 引用 SHALL 已转交 slot/submission。prepare workers SHALL 不共享 GetFrameAllocator 的可变分配游标。

#### Scenario: GPU slot 尚未空闲
- **WHEN** 上一 GPU 帧仍在途，但下一 CPU packet 可以安全应用
- **THEN** 可以用 CPU 私有 scratch 完成 prepare；GPU allocation/record 等待 lease，不覆写旧 slot

#### Scenario: 录制完成后释放 CPU 图
- **WHEN** graph/compiled consumers 已退出且提交所需 GPU refs 已移交
- **THEN** 可先析构 CPU graph 和回调再 reset build arena，不必固定等待 GPU 完成；GPU 资源仍由 slot 保活

### Requirement: 复用批次 dispatcher 与独立 prepare pool

ScenePrepareScheduler SHALL 保持 Core 依赖编译与具体执行器分离；批次执行优先适配现有 DeviceFrameDispatcher，提交至 renderer 专用 CPU prepare pool，并补失败收齐和有界 fan-out 进展性。DeviceFrameContext 的 parallelNum/后端 ThreadContext SHALL 仅表示 GPU 编码并行；parallelNum=1 的串行参考模式 SHALL 不妨碍 CPU prepare 多 worker 并行。prepare 与编码 SHALL 使用不同批次、pool 和生命周期，不混用同一个进行中的 dispatcher。

#### Scenario: 多 worker prepare 与串行录制
- **WHEN** prepare pool 有多个 worker 而 DeviceFrameContext.parallelNum 为 1
- **THEN** CPU systems/chunks 可按依赖并行，GPU 录制由对应 FrameContext 的串行计划完成，不额外创建编码 worker pool

### Requirement: 输出失败与安全关闭

Scheduler SHALL 在建图前获取可用输出，将成功结果作为 borrowed lease 导入 RDG。单输出失败 SHALL 不取消其它有效输出和场景命令。已 acquire 后发生失败 SHALL 通过有效的收尾提交或后端错误退休路径处理，不得 present 等待未 signal 的 semaphore。DestroyViewport/Shutdown SHALL 在 GPU 与 presentation 资源退休后确认；宿主 SHALL 在确认后销毁原生窗口，等待期间继续处理平台事件。
Scheduler SHALL 先取输出配置快照，用 CPU 私有 scratch prepare，再取得可复用 slot 并有界 acquire；GPU 帧分配不得提前覆写未退休 slot。等待 SHALL 保证控制事件进展，不越过冻结边界改 ECS；extent/generation 改变 SHALL 重做受影响 View 或跳过对应输出。

#### Scenario: 关闭一个在途窗口
- **WHEN** 两个窗口渲染期间关闭其中一个
- **THEN** 另一个窗口继续工作；被关闭窗口停止新 acquire，等待其 GPU 与 presentation 使用退休后返回销毁确认

#### Scenario: prepare 后输出尺寸变化
- **WHEN** View prepare 后 acquire 的实际 extent/generation 与快照不同
- **THEN** 不沿用不匹配的 ViewContext；重做受影响准备或跳过该输出，等待期间仍处理 closing intent

### Requirement: ECS 增量构建与输入版本一致

Scene SHALL 持久保存 registry，使用稳定且带 generation 的外部 entity 映射，并在帧边界串行增量更新。输入组件、派生组件、Scene/View 帧结果和 GPU slot 数据 SHALL 分离。资源绑定 SHALL 使用精确版本键，dirty stamp SHALL 覆盖派生结果的实际输入版本，并仅在成功生成后推进。metadata、bounds、实际绘制 mesh SHALL 属于同一选定版本；worker SHALL 不持可变 GPU cache 或 RHI 指针。

#### Scenario: 热更新回退保持几何一致
- **WHEN** 新 mesh revision 尚未上传而策略选择旧 resident 版本
- **THEN** 冻结前选定旧版本 metadata/backing，bounds/cull/DrawItem 使用一致版本；不能在晚期解析时更换 mesh 却沿用另一版本的裁剪结果

#### Scenario: deferred 与外部删除同一边界到达
- **WHEN** 上一帧 deferred 命令请求更新实体，而本包随后销毁它
- **THEN** deferred 先应用、本包按序应用，最终实体被删除；后续过期 generation 更新不能使其复活

### Requirement: FrameGraph 构建产物与扩展边界

Renderer SHALL 在 coordinator 上用冻结的 PreparedFrame、有效输出租约和 UploadBatch 构建一个当帧 RDG。构建 SHALL 先固定资源版本解析和共享上传，再加入逐 View passes 与 FrameComposer，最后 Freeze 并交由后端 Compile/FrameContext Dispatch，收齐 RecordedFrame 后显式 Submit。Frame feature SHALL 通过消费方 engine 接口注册，不能从 callback 直接提交队列或访问宿主/可变 ECS。UI 快照 SHALL 使用 logical texture/ViewOutput ID 及 generation/revision，在本图解析并保留资源 lease。

#### Scenario: UI 快照引用已 resize 的 ViewOutput
- **WHEN** UI 快照产生后对应输出 generation 已变化
- **THEN** 旧引用按声明策略占位或跳过，不使用过期 Image 指针或其它图的 RDG handle

#### Scenario: feature 停用与卸载
- **WHEN** 仍有 jobs、录制回调、continuations 或 GPU leases 引用 feature
- **THEN** 停用阻止未来建图，模块卸载等待所有相关使用退出

### Requirement: 资源 ticket 版本与订阅

Renderer SHALL 接收拥有数据的 CPU resource payload，按资源身份、content revision、usage/variant 和 device epoch 去重 ticket；订阅 SHALL 单独保存 Scene/Entity generation、bindingRevision 与 request sequence。GPU 创建和状态更新 SHALL 由 coordinator 执行。完整资产 IO/解码仍归宿主资产服务，不进入 prepare kernel 或 Renderer 的 Framework 依赖。

#### Scenario: 多 Scene 共享请求且一个实体销毁
- **WHEN** 两个 Scene 订阅同一资源 ticket，而其中一个 entity 销毁
- **THEN** 只移除该订阅，其它订阅继续；迟到结果校验各自 generation/bindingRevision 后应用，不重复创建相同 GPU 版本

### Requirement: 资源 Resident 与 InGraph 使用资格

资源可用性 SHALL 区分已完成上传且状态已知的 Resident、由当前图内 producer 保证的 InGraph 和 Unavailable。首版 SHALL 用同 graphics queue 的 inline RDG copy 提供 InGraph 使用，消费者声明必要访问/barrier；InGraph SHALL 带 GraphId 且不能跨图使用。提交前不得将 ticket 标成全局 Resident；上传完成、必需范围已初始化及相关状态/ownership 就绪后才能发布。几何、材质依赖和 PSO 兼容性 SHALL 在生成 DrawItem 前检查。

#### Scenario: 同帧新网格上传并绘制
- **WHEN** CpuReady 的 vertex/index payload 被接纳进本图上传预算，且所需 shader/PSO 可用
- **THEN** 本图 copy 先于对应 draw 并有显式访问依赖，CPU 不同步等 GPU 上传；其它图不能复用本图 InGraph 令牌

#### Scenario: 资源延后或上一上传仍在途
- **WHEN** CPU prepare 已选定版本但本图上传预算不足，或该版本在另一 submission 上传尚未完成
- **THEN** 首版跳过或使用已正确准备的回退，不阻塞整个 SceneReady，不在 ResourceFinalize 偷换几何版本

### Requirement: 有界上传与可验证发布

资源调度 SHALL 限制 pending CPU bytes、staging bytes、创建数量和在途上传量。接纳的持久上传 SHALL 为显式图 root，并可在没有窗口输出时形成 upload-only submission。Compile/Record SHALL 只生成 tentative receipts；仅实际录制且提交 Accepted 的 copy 可进入 UploadSubmitted。Resident 发布 SHALL 等待成功完成；Rejected、DeviceLost/不确定错误 SHALL 分别回滚或隔离，不推进错误的资源状态账本。

#### Scenario: 窗口全最小化仍有上传
- **WHEN** 没有可用窗口输出但有预算内的持久资源上传
- **THEN** 可以提交不含 Present 的 RDG，copy 不因无 View consumer 被裁剪；仅真正无 GPU 工作时视为空帧

#### Scenario: 图剔除或提交失败
- **WHEN** 某 upload 未进入 compiled 活跃 passes，或提交已知被拒绝
- **THEN** 不为它发布上传成功/Resident；可重试 CPU 数据保留，未提交的状态增量不提交到账本

### Requirement: 资源热更新与取消退休

本帧 SHALL pin 精确资源版本；新版本 SHALL 不原地覆写在途 geometry/descriptor/metadata。旧版本 SHALL 保活至所有 CPU/GPU leases 退休；取消只停止后续工作/发布，不提前释放仍被 jobs 或 GPU 读取的内存。CPU 原始 bytes 与 staging SHALL 使用独立释放条件。

#### Scenario: 上传过程中取消并热更新
- **WHEN** 旧 revision 的上传已提交且新 revision 或取消到达
- **THEN** 旧 staging/backing 仍存活到真实完成；旧结果不覆盖新请求，已有合法 consumer 仍持有其精确版本直到退休

### Requirement: 最小真实出图验收

主体 SHALL 使用程序化 indexed mesh、明确的 VS/FS program、真实 PSO、DrawItem 和常量绑定提供可执行验证。验收 SHALL 包括同 Scene 两相机、不同尺寸的窗口/离屏输出、第二个 Scene、实体变换/删除和至少两个 GPU inflight slots。仅 clear 或只检查图结构 SHALL 不构成通过。

#### Scenario: Vulkan 与 DX12 验证
- **WHEN** 最小样例分别运行在 Vulkan 和 DX12
- **THEN** 实际输出或 readback 显示两个相机各自正确的网格结果；连续变换和删除不出现跨视图污染或已删除实体残留

#### Scenario: 未完成平台验证
- **WHEN** Metal surface 线程和 drawable 协议尚未通过平台 probe
- **THEN** 明确记录未迁移状态，保留可选旧入口且不同时启用两套设备宿主，不宣称 Metal 迁移验收完成
