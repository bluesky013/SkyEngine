## ADDED Requirements

### Requirement: Renderer 对象与模块边界

Aurora SHALL 作为 foundation 保留在 `engine/aurora`，不得注册为 Aurora plugin。系统 SHALL 通过 `IRenderer` 接口提供渲染能力；实现 SHALL 复用既有 `core`，Framework 对接归 `adaptor`，模块入口归 `runtime`，不得再建立 `renderer/{core,adaptor,runtime}` 子模块。公共接口 target SHALL 不链接具体实现。接口 SHALL 只引用句柄、描述、命令和结果数据；不得暴露 World/Actor、具体 plugin 或可变渲染 ECS。活动宿主 SHALL 共享一个 Renderer/Device，GPU 资源缓存 SHALL 按 device 共享。

宿主 SHALL 通过 `IRendererProvider` 创建对象，创建结果 SHALL 为明确错误或非空、已初始化的 Renderer；未实现的运行时或不支持的新后端帧协议不得伪报成功。成功对象 SHALL 提供已解析 backend 和本实例稳定的非零 deviceEpoch；首版 device loss SHALL 进入失败/关闭流程，新实例 SHALL 使用新 epoch。公共接口消费端 SHALL 不链接 provider 实现或 RHI.Static；provider module SHALL 通过共享 Environment 发布服务，不能覆盖另一活动 provider 的注册。

#### Scenario: 游戏与编辑器使用同一接口
- **WHEN** 分别启动游戏宿主或编辑器宿主
- **THEN** 各自通过接口解析 Renderer provider，且每个进程只创建一个活动渲染设备；宿主不链接 provider 的具体实现

#### Scenario: 接口与模块注册独立验证
- **WHEN** 消费端仅链接接口 target，或通过动态/静态模块入口注册并卸载 provider
- **THEN** 公共头可独立编译链接；宿主可解析同一服务，重复注册不覆盖原 owner，卸载只清除自身注册且允许后续重新加载

#### Scenario: 生产创建与阶段贯通
- **WHEN** 支持的平台通过 provider 成功创建 Renderer，并提交包含场景更新、资源载荷和有效 View 输出的 owned packet
- **THEN** 实例贯通 Apply、prepare、资源解析、后端 Compile/Dispatch/Submit 与完成事件；成功创建不能只返回占位对象，关闭会收齐 CPU 借用、GPU 和输出责任

### Requirement: 生产帧组合与事件

成功创建的 Renderer SHALL 复用唯一 CpuRenderCoordinator、ScenePrepareScheduler、ResourceCatalog/ResourceUploader、FrameScheduler 和 DeviceFrameContext；不得额外建立 GPU ring 或对同一个 context 使用多个 completion poller。Create SHALL 等待 coordinator 初始化结果，Device 与 native caches SHALL 在该线程创建和释放，backend module SHALL 保活到 context 及 GPU 引用退出。主线程 SHALL 持有 provider module，收到 RendererStopped 并销毁 Renderer 后才卸载。

FrameBuildContext SHALL 拥有 PreparedFrame、精确资源版本 pins、mesh/material layout、输出配置版本与回读请求；不得跨下一次 Apply 保存 live ECS 访问。CPU prepare SHALL 先于 TryBeginSlot，acquire SHALL 位于 output-pass 准备阶段。资源上传与输出 SHALL 使用同一个 RecordedFrame/Submit，并同时验证 upload receipts 与输出同步责任；只有真实完成才发布 Resident。FrameCompleted SHALL 表示该 packet 的 GPU 工作已完成或没有 GPU 工作，不表示 presentation 已退休。

#### Scenario: 首次候选与预算延后
- **WHEN** packet 包含 owned mesh/material CPU 载荷且尚无兼容 Resident
- **THEN** prepare 可冻结候选 metadata；GPU finalize 仅在该精确版本已 Resident 或本图完整上传可解析时绘制，预算不足只省略该 draw，不能替换已经冻结的 mesh/bounds

#### Scenario: 热更新与共享取消
- **WHEN** 新版本等待上传、多个实体共享 ticket，或最后一个订阅者被删除
- **THEN** 冻结前可选择兼容旧 Resident，已冻结帧保留其 layout/backing；删除一个订阅者不取消共享上传，最后一个订阅者取消会发布状态，在途 GPU 引用仍等待完成；显式重新提供失败版本的 owned payload 会创建新 ticket attempt

#### Scenario: 离屏 resize 排队
- **WHEN** 多个 CPU 帧在等待 GPU slot 期间连续 resize 或销毁同一离屏输出
- **THEN** 每个旧帧继续持有自身输出版本，不读取后续 packet 的尺寸或 image；销毁确认等待此前可能引用该对象的帧，但不等待后续无关帧

#### Scenario: Scene prepare 可恢复失败
- **WHEN** 某 Scene 的派生 bounds/light 或 View projection 计算显式失败
- **THEN** 收齐 jobs/借用并报告 ScenePrepareFailed，暂停受影响 Scene；后续 packet 可处理其它 Scene，受影响 Scene 的有效实体或 View 更新允许重新准备；不能发布失败 system 的部分结果

#### Scenario: GPU frame 错误与设备错误
- **WHEN** 编码失败但设备仍可用，或收到 DeviceLost/不确定提交等终态错误
- **THEN** 前者丢弃本帧并允许后续帧继续；后者拒绝新输入，为已接受包给出结果、收齐 CPU/GPU/输出责任并报告 RendererStopped，下一实例使用不同 epoch

#### Scenario: 有界公共回读
- **WHEN** packet 请求有效 RGBA8 离屏 Viewport 的回读
- **THEN** 图内 copy 保活到 GPU 完成后才发布拥有 bytes 的 ViewportReadback；请求数量和总 bytes 受预算约束，非法输出或格式显式失败，不把 mapped 指针交给主线程

### Requirement: Scene View Viewport 独立生命周期

Renderer SHALL 独立管理 Scene、View 和 Viewport。Scene SHALL 保存 ECS 场景状态；View SHALL 引用一个 Scene，保存独立相机、过滤、可见集合、LOD 和历史状态；Viewport SHALL 描述原生窗口或离屏输出。首版每个场景 View SHALL 关联一个输出，原生窗口 SHALL 可合成多个离屏 View。Scene、Entity、View、Viewport IDs SHALL 包含 generation，销毁确认前不得复用槽位，generation 耗尽时 SHALL 退休槽位。

#### Scenario: 同场景双相机
- **WHEN** 两个 View 引用同一 Scene 并使用不同相机和输出尺寸
- **THEN** mesh/material GPU 资源共享，两套相机、裁剪、LOD、历史与输出尺寸相互独立

#### Scenario: Scene 销毁与迟到更新
- **WHEN** Scene 被销毁且旧 generation 的实体或 View 更新随后到达
- **THEN** 更新被拒绝，相关 View 停止场景绘制并报告状态，原生窗口不被隐式销毁

#### Scenario: CPU 删除与最终退休分离
- **WHEN** CPU 状态已删除，但 coordinator 尚未确认相应 borrowers、GPU 与 presentation 使用结束
- **THEN** 仅产生待退休记录，不发布最终销毁确认；相同槽位的新 generation 创建被跳过并报告，确认后只允许更大的 generation，最大 generation 的槽位永久退休

#### Scenario: 旧 View 不自动绑定新 Scene 或输出
- **WHEN** Scene 或 Viewport 槽位以新 generation 重建
- **THEN** 旧 View 仍保留原引用并暂停，必须经有效 UpdateView 显式重绑后才恢复；零尺寸输出仅暂停自身 View，不影响其它输出

### Requirement: FramePacket 线程与所有权协议

World/UI SHALL 留在宿主主线程，渲染 ECS、Device 与 GPU caches SHALL 由专用渲染线程持有和协调。CPU prepare worker SHALL 仅通过已声明的受限访问借用 ECS/快照；结构变更、GPU 对象/cache 修改、逻辑建图与队列提交 SHALL 由 coordinator 控制。GPU 命令编码 SHALL 由匹配后端的 FrameContext 按计划在 coordinator/encode workers 执行，使用独占编码资源与已发布绑定，不访问 live ECS。主线程 writer SHALL 提交拥有数据的 FramePacket；资产 worker 结果 SHALL 先在主线程归并。包 SHALL 携带顺序号及有序生命周期/更新命令，按 FIFO 处理。载荷 SHALL 按值持有或明确转移不可变所有权；CPU 源数据 SHALL 保活到全部读取/完整 staging 拷贝完成，staging SHALL 保活到 GPU 上传完成。CPU 待处理包上限 SHALL 与 GPU inflight slot 数分开配置。

#### Scenario: 队列已满
- **WHEN** 主线程 TrySubmit 遇到有界队列满
- **THEN** 返回 Busy 且原包仍归调用方；宿主继续处理平台事件后重试，不静默丢弃 create/destroy 命令

#### Scenario: 提交所有权可观察
- **WHEN** TrySubmit 返回 Accepted、Busy 或 Rejected
- **THEN** Accepted 消费并清空调用方包；Busy/Rejected 保留包不变，Accepted 本身不证明 CPU 命令、GPU 或 presentation 已完成

#### Scenario: 通道关闭与消费唤醒
- **WHEN** CPU packet 通道关闭，或 consumer 正等待空队列
- **THEN** 关闭立即拒绝后续提交并唤醒 consumer；已经接收的包仍按 FIFO 交付，只有关闭且排空后才报告通道结束，所有 consumer 退出后才能销毁通道

#### Scenario: 提交序号不因 Busy 消耗
- **WHEN** 非零递增序号的包因队列满而返回 Busy，随后同包重试
- **THEN** Busy 不推进已接收序号，重试可被接收；重复、倒序、零序号和回绕包被拒绝且保留调用方所有权

#### Scenario: CPU 结果消费缓慢
- **WHEN** 已接收包的 CPU 结果尚未被宿主消费，结果交付预算耗尽
- **THEN** 后续 TrySubmit 返回 Busy 并保留包；已接收包已有结果容量，不丢结果、不以宿主读事件作为停机前提，停止事件在全部 packet 结果之后只交付一次

#### Scenario: CPU prepare 失败后的 accepted 包
- **WHEN** CPU prepare 失败而后续包已经被接收，且调用方请求 shutdown
- **THEN** 停止接收新包，收齐当前 kernels/借用，为每个 accepted 包交付执行结果或明确未执行失败；不能直接清空队列或提前报告停止

#### Scenario: CPU 初始化与计划构建失败
- **WHEN** packet/result 容量或 prepare worker 数为零、实例重复初始化，或 prepare builder 返回错误
- **THEN** Init 显式拒绝非法配置且不创建 worker；builder 错误进入当前包结果并保留错误码和诊断，随后按失败 drain 协议关闭；未初始化实例可安全销毁并拒绝提交

#### Scenario: 异步资产过期
- **WHEN** 资产结果的 entity generation 或 asset revision 与当前状态不符
- **THEN** 渲染线程不应用该结果，并安全回收其载荷

### Requirement: CPU 命令预检与持久状态

CPU 场景状态 SHALL 持久保存 ECS 和外部 entity 映射，不因空包或多个 View 重建。命令 SHALL 按包内顺序预检，并在同一 generation 内使用严格递增的非零 Update/Resize revision，Create 基线为零。过期句柄、重复/迟到 revision、退休待确认复用 SHALL 跳过对应命令并报告其索引与原因，不阻断同包其它有效生命周期命令。格式无效或缺少处理器的 payload/extension SHALL 在业务状态修改前拒绝整包。已进入 Apply 的合法序号 SHALL 被消耗，拒绝内容不允许同序号重放。

#### Scenario: 无效包不部分应用
- **WHEN** 一个包前部包含有效删除命令，后部包含非法描述或尚未接入的 payload
- **THEN** 整包报告失败，业务状态保持原状，不返回前部删除的退休记录，后续更大序号的合法包可继续应用

#### Scenario: 迟到更新不撤销同包删除
- **WHEN** 实体删除后，同包还包含旧 generation 的更新，或较新 revision 后又出现旧 revision
- **THEN** 迟到更新按命令索引报告并跳过，已生效删除或新 revision 保留，旧实体不能复活

#### Scenario: 持久状态提供一次 Scene 与独立 View 输入
- **WHEN** 多个有效 View 引用同一 Scene，随后空包或实体增量包到达
- **THEN** registry 与存活 entity 映射保持稳定，本轮 prepare 输入中 Scene 只出现一次，各 View 保留独立相机、过滤与输出描述；删除后所有 View 均不能读到旧实体

### Requirement: 冻结 CPU 输入与读取借用

冻结的 CPU 输入 SHALL 记录最近成功应用的 packet 序号，不得把拒绝内容的序号当作新状态。Scene 输入存储 SHALL 由冻结 lease 保活，上下文及派生读取视图 SHALL 延长该 lease。kernel 不得取得可变 registry；组件读取 SHALL 按 system 已声明的类型、domain 和 owner/generation 校验并返回 const。整个计划的上下文 SHALL 在第一个 kernel 执行前完成校验。CPU 下一包修改、实体/场景删除、退休确认 SHALL 等待所有借用退出，kernel 回调返回或 shutdown 请求本身不代表借用结束。

#### Scenario: 回调已返回但只读输入视图仍被持有
- **WHEN** prepare 回调返回后某 consumer 仍持有从 context 取得的只读输入视图，同时下一包请求更新或删除该实体
- **THEN** 原组件保持有效，下一包及停止完成等待该视图释放；直接被冻结阻挡的 Apply 不消耗包序号，释放后可用同序号继续

#### Scenario: 绑定错误阻止部分 kernel 执行
- **WHEN** 计划后部的 system 声明了错误 owner、未知组件或当前输入层不支持的 Write
- **THEN** Bind 返回无 context 的错误结果，并在运行任何 kernel 前报告绑定失败；kernel 内尝试读取未声明组件返回无效视图并标记该 system 失败

#### Scenario: 拒绝包不改变冻结版本
- **WHEN** 成功应用包 A 后包 B 内容预检失败，随后取得现有状态的冻结视图
- **THEN** 冻结视图仍标记 A 的成功序号，既不发布 B 的部分状态，也不把 B 当作已应用状态

### Requirement: 派生组件的受限写入与成功发布

输入组件 SHALL 仅由 ApplyCommands 写入；派生组件的池和实体项 SHALL 在冻结前创建，kernel SHALL 只通过声明的 WriteView 修改派生值，不取得 registry、任意实体写入入口或结构修改能力。组件指针和稳定行序 SHALL 在第一个 kernel 前绑定，Slice SHALL 只能缩小父视图范围，边界检查 SHALL 防止整数溢出。派生组件的 Read/Write 权限 SHALL 限于该节点的同步 kernel 执行期和执行线程；保存 context/view 不得延长该权限，组件引用不得逃逸迭代回调。只读输入借用仍可延长帧 lease；跨节点消费者 SHALL 通过自己的声明重新取得派生读取权限，或消费独立复制的值。

首个 CPU 对象提取组件 SHALL 区分 value、输入 revision、成功准备 sequence 和 valid，创建时 revision 0 也必须执行首次准备。有效输入更新 SHALL 使缓存失效，迟到更新不得影响缓存；输入未变时可复用成功版本。写入候选 SHALL 在前驱发布后初始化，并只在当前 kernel 完整成功返回后发布；显式 Fail 或非法受限访问 SHALL 丢弃当前节点全部候选，阻止下游消费。该 stamp 仅覆盖已声明的实体输入，不代表资源 metadata、bounds 或 GPU residency 已准备。

#### Scenario: 写后读与连续 writer
- **WHEN** 已编译依赖计划先执行 writer A，再执行 writer B 和 reader
- **THEN** B 的候选以 A 已发布的结果为起点，reader 在相应依赖成功后读取新值；不能在全计划绑定时提前复制旧值导致 B 覆盖 A

#### Scenario: 派生访问权限随 kernel 结束
- **WHEN** 视图被保存后交给另一线程，或在后继节点内再次访问
- **THEN** 派生 Read/Write 返回无效视图或 false 且不访问存储；活跃 scope 的跨线程访问标记生产者失败，已结束 scope 不重新打开权限或改变发布结果；帧重复执行被拒绝

#### Scenario: 范围内准备与版本失效
- **WHEN** kernel 仅写某个 Slice，随后实体更新或以新 generation 重建
- **THEN** 本次仅该范围的数据和 stamp 可发布，空范围无写入，越界或扩大父范围失败；更新令对应缓存失效，新 generation 不继承旧缓存

#### Scenario: 候选完成后 kernel 失败
- **WHEN** kernel 已生成部分或全部候选，但在返回前调用 context.Fail、越权访问或请求越界 Slice
- **THEN** 当前节点不发布任何候选或成功 stamp，迭代返回 false，依赖节点跳过而独立节点可完成；任务结束后保留的输入 ReadView 仍可读取并保活

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

分块 SHALL 显式声明 owner 与区间大小，并只为可证明互斥的访问创建 chunk 上下文；不支持的跨 owner 或共享写入 SHALL 在提交前拒绝。所有 chunks SHALL 持独立访问作用域、受限行表和有界私有 scratch。派生候选与成功 stamp SHALL 在完整 system 成功后统一发布；任一 chunk 失败 SHALL 保留原值/stamp、跳过消费者，并收齐其它 chunks。多个失败诊断 SHALL 按稳定 chunk 顺序选择。

#### Scenario: 单 system 多 chunk
- **WHEN** WorldBounds system 被切为多个不重叠实体区间
- **THEN** 各区间可并行写已有 Bounds，消费者等待全部区间完成；chunk 不得借任意实体查询写入其它区间

#### Scenario: 分块候选不能提前发布
- **WHEN** 一部分 chunks 已生成候选，另一个 chunk 仍未退出或最终失败
- **THEN** 在完整 system 成功前不更新实际派生值及 stamp；失败则本 system 的所有候选均不发布，成功则按稳定 chunk/行顺序发布后释放消费者

#### Scenario: 分块私有 scratch 与范围
- **WHEN** 两个 chunks 同时取得 Read/Write 和 scratch，或调用方保存 context 后跨线程/完成后再次请求 scratch
- **THEN** 两个 chunks 的行范围和可写 scratch 不重叠；重新获取 view 不能扩大区间，非法时机不能取得可写 scratch，保留的输入只读视图继续保活存储

#### Scenario: 多个分块写者按完整 system 依赖执行
- **WHEN** 两个写者连续修改相同 Scene 的派生池，后继也被分块
- **THEN** 第二个写者的每个 chunk 在第一个 system 发布后初始化候选；所有读者在第二个 system 完整成功后读取结果

#### Scenario: 稳定输出与延迟结构命令
- **WHEN** worker 完成顺序变化，且某 chunk 请求创建或销毁已注册渲染实体及其固定组件集合
- **THEN** 当前帧池地址和查询集合保持不变；输出与命令按固定 system/chunk 顺序归并，命令在下一帧边界应用

任务输出 SHALL 使用启动前预留的私有容量与非零 channel，复制拥有数据的无指针 byte payload。整个批次成功并收齐任务后 SHALL 按稳定 system/chunk/emission 顺序合并为不借用 ECS 的只读产品；消费者按此顺序 reduce。任何任务失败 SHALL 不发布该批通用输出或 deferred。首版 deferred SHALL 限定已注册渲染实体的创建、更新、销毁；更新/销毁 SHALL 只针对本任务绑定行，并捕获输入 generation/revision，创建 ID SHALL 由调用方提供。任意组件类型的动态增删不属于首版命令集。

#### Scenario: owned 输出与稳定 reduce
- **WHEN** chunks 乱序完成或切换到串行 fallback，kernel 的源 byte buffer 已改变且帧存储已释放
- **THEN** 输出顺序、复制的值和依该顺序计算的 reduce 结果一致；输出可独立保活，不延长 ECS 冻结

#### Scenario: 输出容量与调用范围
- **WHEN** 配置超出批次 records/bytes/deferred 上限、乘法存在溢出风险，或 kernel 超过自身配额
- **THEN** 前者在启动前拒绝；后者显式失败并丢弃批次产品；其它线程或 kernel 完成后不能追加

#### Scenario: deferred 与 packet 一起预检
- **WHEN** 上一成功帧有 deferred，下一 packet 含无效命令，之后又收到有效 packet
- **THEN** 无效包不能部分应用 deferred，pending 保留到有效包；有效包按 deferred 前缀再外部命令应用，整批只消费一次且拒绝重放

#### Scenario: deferred 与外部更新的 revision
- **WHEN** 同一边界多个 deferred 引用相同输入 revision，且外部 packet 又提交合法的新实体输入
- **THEN** deferred 前置条件按边界入口校验；新宿主输入按入口 revision 判定后覆盖候选，本包后续输入仍递增，旧 revision 被跳过

#### Scenario: deferred 的范围与迟到诊断
- **WHEN** kernel 尝试更新/删除其它 chunk 的实体，或 Apply 遇到过期 generation/revision、未退休的复用 ID
- **THEN** 越界 kernel 显式失败；迟到命令跳过且诊断区分 deferred/packet 索引，不复活实体或绕过退休

#### Scenario: 关闭时没有下一 Apply
- **WHEN** 最后一个 accepted packet 的 prepare 生成 deferred，而宿主已关闭入口
- **THEN** coordinator 排空 jobs 和 borrowers，不产生虚构帧；停止结果报告尚未应用的命令数量

### Requirement: Prepare 执行器完成与失败协议

CPU prepare SHALL 复用现有 ThreadPool 实现，通过每帧 JobGroup 跟踪完成。首版 SHALL 使用 Renderer 持有的专用 prepare pool，所有 Scene/View 共用，并限制整个活动计划节点数以满足该池队列的进展预算。worker SHALL 不阻塞等待同池子任务；依赖 SHALL 在提交前建完。空计划 SHALL 立即完成。CPU 下一帧的 live ECS 修改、Scene 销毁、CPU arena reset 和 module 卸载 SHALL 等待引用它们的 jobs 全部退出。任务失败 SHALL 通过显式结果记录并正常释放完成依赖，不使用 throw/try/catch，下游跳过无效结果；失败 Scene SHALL 暂停并报告，恢复需重建，不发布半完成状态。

#### Scenario: 空计划与单 worker
- **WHEN** Scene 没有可处理实体，或用单 worker 执行带依赖及大量逻辑 chunks 的计划
- **THEN** 空计划直接完成；非空计划在预算内批处理并完成，不因零 chunk future、队列饱和或嵌套等待停滞

#### Scenario: system DAG 超过节点预算
- **WHEN** 计划在 hard 任务/scratch 限额内，但 system 或展开后的 chunk/join 节点数超过可安全并行提交的预算
- **THEN** 使用相同依赖顺序的串行执行回退并完成，不向有界队列无上限提交

#### Scenario: 分块预算包含汇合节点且回退保持分块语义
- **WHEN** 单 worker 的 chunks 加 join 恰好达到队列预算，或再多一个节点
- **THEN** 预算内批次可完成，超出时向同一专用 pool 提交一个任务，使用相同分区、scratch 和完整 system 发布逻辑串行执行；coordinator 可订阅和请求取消

#### Scenario: hard 任务与 scratch 限额
- **WHEN** 分块计划超过 hard 节点总数或 scratch 总字节数，或预算计算存在溢出风险
- **THEN** 提交返回明确错误，任何 kernel 都不执行；空行表使用一个空 chunk 和 join，空计划直接完成

#### Scenario: 任务失败时关闭 Scene
- **WHEN** 某 prepare job 失败且宿主同时请求关闭该 Scene
- **THEN** 所有已启动 jobs 完成或跳过后才回收其内存和回调，Scene 不继续发布部分 prepare 结果

#### Scenario: 串行参考执行
- **WHEN** 使用同一已编译 DependencyPlan 在 coordinator 上执行参考路径
- **THEN** kernels 按稳定依赖序运行，显式失败被记录，失败节点的传递消费者被跳过，独立节点仍完成；空计划直接成功

### Requirement: Prepare 逻辑完成与恢复上下文

Prepare JobGroup SHALL 复用 async-threadpool 定义的通用 Completion 与指定 executor 交付协议；coordinator SHALL 能检查状态或订阅 continuation，不依赖阻塞 future.get 判断逻辑完成。全部已启动 kernels 退出、未启动节点被收齐后才能发布最终完成/取消结果。CPU consumers/continuations 的借用 SHALL 独立保活到退出；取消 SHALL 不被当作底层 jobs/GPU 工作已退休的证明。首版 prepare kernel SHALL 一次执行至完成，不挂起，不持有 ECS 访问或任务临时存储跨异步等待；尚未完成的 coroutine SHALL 不被当作普通 TaskNode 已完成的结果。

#### Scenario: worker 完成后恢复协调逻辑
- **WHEN** 最后一个 worker 完成 JobGroup，而后续步骤需要访问 GPU cache 或构建 RDG
- **THEN** 结果只发布一次，后续步骤排队回 render coordinator，不能在该 worker 上直接执行

#### Scenario: 订阅已完成任务或同时取消
- **WHEN** continuation 注册时任务已完成，或取消与完成通知并发
- **THEN** 按完成状态协议恰好交付一次结果或取消结果；捕获对象在相关 jobs 与通知退出前保活，不重复恢复或提前释放

#### Scenario: continuation 交付预算耗尽
- **WHEN** 指定 executor 的订阅交付容量不足
- **THEN** Subscribe 返回 Busy 并保留调用方 callback；Accepted 必须已保留交付容量和 executor/module 生命周期，之后不能因普通 worker 队列满而丢弃通知

#### Scenario: CPU 取消与正常关闭
- **WHEN** host 请求取消当前 prepare，或只请求 graceful shutdown
- **THEN** 取消使未进入的 kernels 跳过，已开始者退出后才发布终态，已有显式失败优先；正常 shutdown 收齐已接受包，不隐式改写成功为取消

#### Scenario: 失败或取消后的 CPU 重建
- **WHEN** 当前 CPU coordinator 收到 Failed/Cancelled，或下一包请求销毁仍有借用的 Scene
- **THEN** 停止继续 prepare 失败状态；等待 kernels、continuations 和 frozen consumers 各自退出后报告已接受包，完整重建状态后才能恢复

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

#### Scenario: 有界图构建 arena 与消费者借用
- **WHEN** graph generation 仍被编译、编码或待提交 receipt 使用，调用方请求 reset
- **THEN** 拒绝 reset；最后消费者退出后按逆序析构非平凡对象再回收内存，builder 本身不额外延长冻结图寿命

#### Scenario: 录制完成后释放 CPU 图
- **WHEN** graph/compiled consumers 已退出且提交所需 GPU refs 已移交
- **THEN** 可先析构 CPU graph 和回调再 reset build arena，不必固定等待 GPU 完成；GPU 资源仍由 slot 保活。待提交 receipt 保留图供上传核验，Rejected 保留重试，Accepted/Indeterminate 后清理 CPU receipt，GPU payload 独立按完成或安全 teardown 退休

### Requirement: 复用批次 dispatcher 与独立 prepare pool

ScenePrepareScheduler SHALL 保持 Core 依赖编译与具体执行器分离；批次执行优先适配现有 DeviceFrameDispatcher，提交至 renderer 专用 CPU prepare pool，并补失败收齐和有界 fan-out 进展性。DeviceFrameContext 的 parallelNum/后端 ThreadContext SHALL 仅表示 GPU 编码并行；parallelNum=1 的串行参考模式 SHALL 不妨碍 CPU prepare 多 worker 并行。prepare 与编码 SHALL 使用不同批次、pool 和生命周期，不混用同一个进行中的 dispatcher。

CPU 执行实现 SHALL 只通过 Core 的通用批次执行能力复用 dispatcher，不引入 Renderer core 对 RHI.Static 的依赖。提交 SHALL 预检所有 bindings 并限制绑定总行数；一个 scheduler SHALL 仅有一个进行中的批次，合法计划超过并行节点预算 SHALL 回退串行参考执行。调度接口和 callback SHALL 不抛异常、不使用 try/catch；kernel SHALL 通过 context.Fail 或 checked result 报告失败，标准库异常当前不处理。

#### Scenario: 多 worker prepare 与串行录制
- **WHEN** prepare pool 有多个 worker 而 DeviceFrameContext.parallelNum 为 1
- **THEN** CPU systems/chunks 可按依赖并行，GPU 录制由对应 FrameContext 的串行计划完成，不额外创建编码 worker pool

#### Scenario: 独立 Scene 并行且读者看到发布结果
- **WHEN** 两个独立 Scene 的对象准备同时就绪，多个读者依赖各自 Scene 的派生组件
- **THEN** 两个准备节点可在不同 CPU workers 重叠执行；读者仅在对应 writer 成功发布后执行，双 View 共享同一 Scene 的准备结果

#### Scenario: 并行节点预算边界与串行回退
- **WHEN** 单 worker 或多 worker 的扇出计划恰好达到独占 pool 的节点预算，或再多一个节点
- **THEN** 预算内批次无队列自锁且每节点执行一次；超出预算走串行参考路径，保持相同依赖、结果与成功 stamp

#### Scenario: 失败传播与内部借用释放
- **WHEN** writer 显式 Fail 或访问校验失败，而独立节点仍在执行
- **THEN** writer 候选不发布，依赖消费者跳过，JobGroup 保持 Pending 直到独立节点退出；完成后释放内部 frame borrows，外部保留的输入 ReadView 仍阻挡下一包

#### Scenario: prepare 提交在执行前失败
- **WHEN** 初始化 worker 数为零、计划成环、绑定无效、绑定总行数超预算或已有批次未完成
- **THEN** 对应 Create/Submit 返回明确错误；拒绝的计划不运行任何 kernel，已运行的批次继续收齐

### Requirement: 输出失败与安全关闭

Scheduler SHALL 在逻辑输出 pass 准备、导入输出 attachment 时获取可用输出，并在 Freeze 前将成功结果作为 borrowed lease 导入 RDG。单输出失败 SHALL 不取消其它有效输出和场景命令。已 acquire 后发生失败 SHALL 通过有效的收尾提交或后端错误退休路径处理，不得 present 等待未 signal 的 semaphore。DestroyViewport/Shutdown SHALL 在 GPU 与 presentation 资源退休后确认；宿主 SHALL 在确认后销毁原生窗口，等待期间继续处理平台事件。
Scheduler SHALL 每帧在 BeginViewport 查询 surface 状态和配置，区分有效、暂不可用、需要更新/重建、surface 失效与查询/设备错误，再取得包含 extent、presentation transform 和输出 revision 的准备快照。预检 SHALL 不 acquire；所需更新/重建 SHALL 等待原输出 CPU/GPU/presentation 责任安全退出，成功后才发布可用配置。能力集合无关变化 SHALL 不触发相同配置的重复重建。
Scheduler SHALL 使用 CPU 私有 scratch prepare，再取得可复用 slot 并在输出 pass 准备阶段有界 acquire；GPU 帧分配不得提前覆写未退休 slot。每个 viewport 每帧成功 acquire 一次并供相关 pass 共享；不依赖输出的构图准备可以先执行。等待 SHALL 保证控制事件进展，不越过冻结边界改 ECS；Freeze 前 extent、transform、surface 身份或输出 revision 改变 SHALL 重做受影响 View 或跳过对应输出，后端 Compile SHALL 不 acquire。
可恢复的 acquire 不可用 SHALL 只跳过当前依赖该 backbuffer 的输出 pass，其它有效 pass 与 command buffer SHALL 继续按正常依赖编码和提交。失败输出 SHALL 不伪造 backbuffer、不加入不存在的 acquire/present 同步、不执行 Present；同一输出的使用者 SHALL 分别验证可用性，不得读取未定义内容。输出列表为空 SHALL 不作为取消整帧的条件，DeviceLost 和真实图错误 SHALL 不降级为普通 pass 跳过。

#### Scenario: 单输出 acquire 失败但其它 GPU 工作有效
- **WHEN** 一个输出 acquire 返回 Busy/OutOfDate，图中还有有效输出、离屏绘制或 copy/upload pass
- **THEN** 只省略失败输出 pass；其它工作正常录制并提交，失败输出不 Present、不等待未 signal 的同步，其它输出仍能呈现

#### Scenario: 全部输出 acquire 失败但离屏工作有效
- **WHEN** 全部输出均无法 acquire，图中仍有独立 roots/side effects 保留的离屏或上传工作
- **THEN** 同一 FrameContext 继续编译、编码和提交，完成通知与读回仍有效；只有无剩余 GPU 工作和提交责任时才释放未提交 lease

#### Scenario: 关闭一个在途窗口
- **WHEN** 两个窗口渲染期间关闭其中一个
- **THEN** 另一个窗口继续工作；被关闭窗口停止新 acquire，等待其 GPU 与 presentation 使用退休后返回销毁确认

#### Scenario: prepare 后输出尺寸变化
- **WHEN** View prepare 后 acquire 的实际 extent/generation 与快照不同
- **THEN** 不沿用不匹配的 ViewContext；重做受影响准备或跳过该输出，等待期间仍处理 closing intent

#### Scenario: 同尺寸旋转与在途重建
- **WHEN** BeginViewport 检测到 surface transform 改变，所选 preTransform/输出映射需要更新，但 extent 不变且旧帧仍在途
- **THEN** 使旧输出配置快照失效，停止该配置的新 acquire 并合并最新重建目标；安全退休和更新成功后，View prepare 与输出 pass 使用同一新 transform 快照，不影响其它窗口

#### Scenario: 预检后仅 transform 改变
- **WHEN** BeginViewport 与 acquire 之间 surface transform 改变而尺寸保持不变
- **THEN** Freeze 前的复核仍拒绝旧 ViewContext；已获取输出走安全 Abandon 或明确的重做路径，不能仅因尺寸相同继续提交

#### Scenario: surface 失效与恢复
- **WHEN** surface 查询报告零尺寸、surface lost 或具体查询错误
- **THEN** 分别暂停输出、通知宿主恢复 surface 或传播错误，不以旧 extent 冒充有效；surface 恢复后重新查询配置并创建输出，不反复 resize 失效的 surface

#### Scenario: 获取后复核查询失败
- **WHEN** acquire 已成功，但 Freeze 前的 surface 复核返回查询错误
- **THEN** 保留具体错误并安全 Abandon 该输出，不取消其它有效 pass；普通查询错误允许下帧重新检查。DeviceLost 则封闭设备的新获取和提交，不作为可恢复窗口跳过

#### Scenario: 延迟 acquire 与无可用输出
- **WHEN** CPU prepare 未结束、没有可分配 slot，或所有输出均 Busy/最小化
- **THEN** prepare 不 acquire、不借用 GPU 分区；取得有效 lease 后每个输出只进行有界尝试。没有 GPU 工作只释放未提交 lease，不 reset fence；有获准上传则在同一 context 提交 upload-only 图

#### Scenario: acquired 输出遗漏或构图失败
- **WHEN** 成功 acquire 的图像未成为录制输出，或构图/编码失败
- **THEN** 不提交缺失输出责任的普通帧，也不 present 等待未 signal 的同步；收齐编码和 CPU 图借用后，通过后端未呈现释放或合法收尾提交完成退休

#### Scenario: GPU 完成但 presentation 尚未退休
- **WHEN** graphics completion 已发布，但输出的 presentation fence/资源交还条件仍 Pending
- **THEN** 其它输出可以继续，原输出保持在途记录，不能 resize、复用对应同步或发出窗口销毁确认；空事件容量不丢失关闭通知

#### Scenario: 提交结果不确定
- **WHEN** output recording 的提交不能证明未入队或正常 Accepted
- **THEN** 隔离输出与提交资源，不能直接 present 或正常确认关闭；保活窗口、Device 和 module，按设备错误 teardown 收尾

### Requirement: ECS 增量构建与输入版本一致

Scene SHALL 持久保存 registry，使用稳定且带 generation 的外部 entity 映射，并在帧边界串行增量更新。输入组件、派生组件、Scene/View 帧结果和 GPU slot 数据 SHALL 分离。资源绑定 SHALL 使用精确版本键，dirty stamp SHALL 覆盖派生结果的实际输入版本，并仅在成功生成后推进。metadata、bounds、实际绘制 mesh SHALL 属于同一选定版本；worker SHALL 不持可变 GPU cache 或 RHI 指针。

#### Scenario: 热更新回退保持几何一致
- **WHEN** 新 mesh revision 尚未上传而策略选择旧 resident 版本
- **THEN** 冻结前选定旧版本 metadata/backing，bounds/cull/DrawItem 使用一致版本；不能在晚期解析时更换 mesh 却沿用另一版本的裁剪结果

#### Scenario: deferred 与外部删除同一边界到达
- **WHEN** 上一帧 deferred 命令请求更新实体，而本包随后销毁它
- **THEN** deferred 先应用、本包按序应用，最终实体被删除；后续过期 generation 更新不能使其复活

#### Scenario: 资源版本到达但实体输入未变
- **WHEN** 新的兼容 Resident mesh 在下一次冻结时替代旧版本，而 entity revision 不变
- **THEN** 派生 bounds/LOD 输入重新生成；旧 PreparedFrame 保持原精确 key 和 metadata，新帧绘制只能解析到新选择对应的 backing

#### Scenario: 未就绪资源不阻塞独立光源
- **WHEN** 实体具有有效点光参数而 mesh 或 material 没有可用版本
- **THEN** 按跳过策略不产生 mesh draw，仍可生成对应 View 可见的 owned light 产品，不在 worker 等待上传

#### Scenario: 无效数值不污染派生输出
- **WHEN** 世界矩阵不是有限仿射变换、光参数非法、LOD 阈值非有限或不递增，或合法输入变换后的 bounds 溢出
- **THEN** 输入校验或 prepare 显式失败，不发布该失败批次的半完成 draw/light 产品

### Requirement: FrameGraph 构建产物与扩展边界

Renderer SHALL 在 coordinator 上用冻结的 PreparedFrame、有效输出租约和 UploadBatch 构建一个当帧 RDG。构建 SHALL 先固定资源版本解析和共享上传，再加入逐 View passes 与 FrameComposer，最后 Freeze 并交由后端 Compile/FrameContext Dispatch，收齐 RecordedFrame 后显式 Submit。Frame feature SHALL 通过消费方 engine 接口注册，不能从 callback 直接提交队列或访问宿主/可变 ECS。UI 快照 SHALL 使用 logical texture/ViewOutput ID 及 generation/revision，在本图解析并保留资源 lease。

#### Scenario: UI 快照引用已 resize 的 ViewOutput
- **WHEN** UI 快照产生后对应输出 generation 已变化
- **THEN** 旧引用按声明策略占位或跳过，不使用过期 Image 指针或其它图的 RDG handle

#### Scenario: feature 停用与卸载
- **WHEN** 仍有 jobs、录制回调、continuations 或 GPU leases 引用 feature
- **THEN** 停用阻止未来建图，模块卸载等待所有相关使用退出

#### Scenario: 停用后重新启用
- **WHEN** feature 停用时有尚未建图的排队帧，随后同一 generation 重新启用
- **THEN** 旧 activation 的回调仍不执行，新帧使用新 activation；已有录制和提交继续退休

#### Scenario: 多 View 写同一输出
- **WHEN** 多个 View 按稳定顺序写同一个 viewport，UI 通过其 ViewOutputRef 采样
- **THEN** 引用指向本帧该 viewport 的合成内容；各相机独立图像使用不同 viewport

#### Scenario: GPU 完成但 presentation 未退休
- **WHEN** feature 请求退休，GPU 已完成而相关窗口的 presentation 尚未结束
- **THEN** module lease 继续保活，OnRetired 和 RenderFeatureRetired 等待最后 presentation 责任退出

#### Scenario: feature 声明窗口却不产生绘制
- **WHEN** acquired output 在最终图中没有存活访问/export，其他输出仍有工作
- **THEN** 只放弃该输出，其他 command buffers 正常提交；实际使用的 borrowed output 仍必须匹配 acquire 和提交同步

#### Scenario: 非法 UI 快照与有效命令同包
- **WHEN** owned UI snapshot 的版本、字节数、索引范围或 clip 无效
- **THEN** 在应用 Scene/Viewport 和 feature 命令前拒绝整包，后续有效 packet 仍正常执行

### Requirement: 资源 ticket 版本与订阅

Renderer SHALL 接收拥有数据的 CPU resource payload，按资源身份、content revision、usage/variant 和 device epoch 去重 ticket；订阅 SHALL 单独保存 Scene/Entity generation、bindingRevision 与 request sequence。GPU 创建和状态更新 SHALL 由 coordinator 执行。完整资产 IO/解码仍归宿主资产服务，不进入 prepare kernel 或 Renderer 的 Framework 依赖。

#### Scenario: 多 Scene 共享请求且一个实体销毁
- **WHEN** 两个 Scene 订阅同一资源 ticket，而其中一个 entity 销毁
- **THEN** 只移除该订阅，其它订阅继续；迟到结果校验各自 generation/bindingRevision 后应用，不重复创建相同 GPU 版本

#### Scenario: 失败重试与迟到回调隔离
- **WHEN** 相同 ResourceKey 在失败后以新 request sequence 显式重试，或相同 epoch 下重建资源目录
- **THEN** 新候选获得独立 catalog/attempt 身份；旧 ticket 的交付、失败和发布不影响新候选，旧 subscriber 取消也不能撤销新订阅

#### Scenario: 有界载荷交付
- **WHEN** 目录的 ticket、subscriber 槽位水位或累计 CPU bytes 达到容量，或 payload 的 key/format/bounds 无效
- **THEN** 返回显式错误且不消费 caller payload；取消释放目录所有权，但已有 CPU 借用继续保活源 bytes，上传拥有者独立统计其仍在途的数据

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

#### Scenario: 完整 image 载荷的范围验证
- **WHEN** CpuReady 候选声明受支持的 RGBA8 2D image、mip 数及 layer 数
- **THEN** owned bytes 按 layer 再 mip 的顺序紧密保存全部子资源；非法尺寸、溢出或 bytes 数不匹配在消费载荷前拒绝，单个 mip 上传不能代表整个版本就绪

#### Scenario: image 分块接纳与完整版本同图消费
- **WHEN** RGBA8 image 候选大于当前帧、在途或 staging 剩余预算
- **THEN** 按紧密 layer/mip/row 字节前缀切成四字节对齐块并生成持久 copy；允许小于一行，续块复用同一 backing，仅创建时计创建预算；只有本图加上此前已完成内容覆盖完整版本时才返回 InGraph handle

#### Scenario: buffer 分块接纳与完整版本消费
- **WHEN** buffer 候选超过当前帧、inflight 或 lease staging 余量
- **THEN** 按四字节对齐前缀分块，首次创建完整大小 backing，后续复用并以 completedBytes 为目标 offset；仅成功完成的前缀进入图入口，最后一块才允许整版本 InGraph，所有块完成才 Resident

#### Scenario: 部分 buffer 不得绕过发布协议
- **WHEN** 部分 buffer 回到 CPU_READY 等待下一块，或真实 backing 只有创建/Accepted 证据
- **THEN** AdoptResident 拒绝提前发布，目录冻结只选择以前完整的版本；无 native backing 且无上传进度的 CPU-only 验证入口不构成真实 GPU 就绪

#### Scenario: 已提交与已完成进度分离
- **WHEN** 一块被 Prepare、Accepted 或成功 PollCompleted
- **THEN** 分别产生候选、增加 submittedBytes、增加 completedBytes；同一版本最多一个未决块，部分完成不进入 Resident 快照，只有完整版本完成后才发布

#### Scenario: 分块数据与 backing 独立保活
- **WHEN** buffer 或 image 部分版本正在上传或等待下一块
- **THEN** CPU payload 至少到最后一块 Accepted，staging 随本块 lease 退休，backing 跨块保留；两种资源共用数量和逻辑 buffer/texel 字节上限，小块不能绕过整个 backing 预算

#### Scenario: 续传失败或更换 owner
- **WHEN** Rejected、Discard、编码失败、取消或 uploader 被销毁
- **THEN** Rejected 保留原录制及候选，Discard/编码失败收齐后保留以前完成前缀；新 ticket 不继承旧进度；新 owner 必须用保留的 CPU 数据从零重建，或显式失败等待重试，不能把旧前缀用在新 backing 上

#### Scenario: 全部 image 回执才允许发布
- **WHEN** image 上传已完成录制并请求提交
- **THEN** 验证本块全部实际 pass 回执、来源与目标 backing、copy 范围、入口完成前缀及最终状态；遗漏本块 copy、篡改有效前缀/范围或其它存活 pass 覆写该版本时拒绝提交，不登记 UploadSubmitted；成功提交也须等全部版本内容完成才发布

#### Scenario: buffer 与 image 共用事务退休
- **WHEN** 一个批次混合 buffer/image，或 image 上传发生取消、提交拒绝或不确定错误
- **THEN** 共用预算、回滚、提交账本与隔离协议；只有对应 token 成功完成才发布精确版本 backing，目录冻结不把 image 当作 buffer 回退

#### Scenario: 窗口全最小化仍有上传
- **WHEN** 没有可用窗口输出但有预算内的持久资源上传
- **THEN** 可以提交不含 Present 的 RDG，copy 不因无 View consumer 被裁剪；仅真正无 GPU 工作时视为空帧

#### Scenario: 图剔除或提交失败
- **WHEN** 某 upload 未进入 compiled 活跃 passes，或提交已知被拒绝
- **THEN** 不为它发布上传成功/Resident；可重试 CPU 数据保留，未提交的状态增量不提交到账本

#### Scenario: 拒绝后原录制重试
- **WHEN** 已录制上传收到确定未入队的 Rejected，随后用原 RecordedFrame 再次提交
- **THEN** Rejected 不消费 CPU payload 或登记提交 token；后续 Accepted 才进入 UploadSubmitted，仍需成功完成才能成为 Resident

#### Scenario: 不确定提交或完成查询失败
- **WHEN** 上传 Submit 为 Indeterminate，或已 Accepted 的上传收到失败的完成结果
- **THEN** 资源不成为 Resident，停止该上传拥有者继续接纳并保留在途预算/原生引用直到安全退休；Accepted 身份不会被当作成功完成，迟到旧 ticket 不能覆盖新 attempt

#### Scenario: 上传 backing 自动纳管与版本转交
- **WHEN** uploader 首次创建 buffer/image backing 并在后续块续传，或完整版本完成后成为 Resident
- **THEN** 首次取得精确 backing 的账本租约，后续块使用 accepted 初态与 token 核验；完成后租约随版本和冻结快照共享，目录不再持有不意味着尚在使用的快照可以提前退休

#### Scenario: 状态条目满额延后
- **WHEN** 账本容量不足以纳管新上传 backing
- **THEN** 返回明确 STATE_TRACKING 延后原因，保留 CPU payload 与原进度；不驱逐被回退版本、计划或在途提交使用的条目，最后 owner 与 GPU 使用退休后可重新接纳

#### Scenario: 放弃或更换上传 owner 的状态退休
- **WHEN** Prepare 失败、Discard、取消、旧 ticket 或 uploader 析构释放 backing owner
- **THEN** 复用原回滚/失败协议且不从析构直接操作 context；未决计划和 GPU 使用继续阻止回收，终态隔离保留到安全 teardown，部分进度不能绑定到下一 owner 新建的 backing

#### Scenario: 一次完成轮询同时返回成功和设备错误
- **WHEN** PollCompleted 返回的完成数组中既有成功上传又有失败回执
- **THEN** 先处理整批错误，不能因成功项在数组靠前就发布 Resident；真正完成的提交按完成退休，错误提交继续隔离

#### Scenario: 分块上传接续混合 image 状态
- **WHEN** 已完成前缀上传的 backing 在同 context 的中间图改变部分 mip/layer 状态
- **THEN** 下一块导入和回执核验使用完整 accepted image 状态，旧的固定 GENERAL 不得冒充初态；每块仍以统一 GENERAL 收尾，完整版本成功完成后才 Resident，不扩大部分内容有效性

#### Scenario: resolve 写入不得冒充上传版本
- **WHEN** 上传录制回执包含额外 raster resolve 写入同一个待发布 image backing
- **THEN** 回执核验拒绝该候选，不推进提交或 Resident；回滚后仍可用原 CPU payload 重试，普通消费者不能借 resolve 改写不可变上传版本

### Requirement: 资源热更新与取消退休

本帧 SHALL pin 精确资源版本；新版本 SHALL 不原地覆写在途 geometry/descriptor/metadata。旧版本 SHALL 保活至所有 CPU/GPU leases 退休；取消只停止后续工作/发布，不提前释放仍被 jobs 或 GPU 读取的内存。CPU 原始 bytes 与 staging SHALL 使用独立释放条件。

#### Scenario: 上传过程中取消并热更新
- **WHEN** 旧 revision 的上传已提交且新 revision 或取消到达
- **THEN** 旧 staging/backing 仍存活到真实完成；旧结果不覆盖新请求，已有合法 consumer 仍持有其精确版本直到退休

#### Scenario: 冻结回退版本保持 CPU 与 GPU 一致
- **WHEN** desired v8 尚未就绪而同 product/兼容 usageVariant/device epoch 的 v7 已 Resident，或 v8 在 prepare 运行中完成
- **THEN** 当前冻结结果成组选用 v7 metadata/bounds/backing，v8 只影响下一次冻结；不能回退到其它资产、布局族或未来 revision；缓存移除后，旧 CPU 数据和 GPU backing 分别保活至对应消费者退休

### Requirement: 最小真实出图验收

主体 SHALL 使用程序化 indexed mesh、明确的 VS/FS program、真实 PSO、DrawItem 和常量绑定提供可执行验证。验收 SHALL 包括同 Scene 两相机、不同尺寸的窗口/离屏输出、第二个 Scene、实体变换/删除和至少两个 GPU inflight slots。仅 clear 或只检查图结构 SHALL 不构成通过。

#### Scenario: Vulkan 与 DX12 验证
- **WHEN** 最小样例分别运行在 Vulkan 和 DX12
- **THEN** 实际输出或 readback 显示两个相机各自正确的网格结果；连续变换和删除不出现跨视图污染或已删除实体残留

#### Scenario: 未完成平台验证
- **WHEN** Metal surface 线程和 drawable 协议尚未通过平台 probe
- **THEN** 明确记录未迁移状态，保留可选旧入口且不同时启用两套设备宿主，不宣称 Metal 迁移验收完成

### Requirement: 扩展资源精确引用与显式释放

FrameExtensionPayload SHALL 声明有界精确资源 key；Renderer SHALL 校验 device epoch，并在 prepare 前冻结对应候选或 Resident。FrameGraphBuildContext SHALL 仅解析该帧已冻结且就绪的图像。standalone 资源 SHALL 支持幂等释放请求，不能提前回收排队帧的候选数据或破坏未决上传回执。

#### Scenario: UI 图像与上传同图使用
- **WHEN** 某精确版本的完整上传被当前图接纳
- **THEN** 该帧可通过受控 context 解析对应 GraphImageHandle，绘制依赖该图上传且沿用同一 FrameContext

#### Scenario: 释放时旧帧仍在准备或上传
- **WHEN** 新 packet 释放旧图像版本而旧帧仍有 CPU 候选或上传借用
- **THEN** 保留必要版本至这些借用结束，已提交 GPU 图继续通过资源 pins 保活，不影响其它 pass 的提交
