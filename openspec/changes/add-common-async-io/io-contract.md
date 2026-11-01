# 异步 IO 与资源准备契约

状态：2026-11-01，供实现评审的确定设计；没有生产代码实现。行业核实与本引擎额外选择分列于 [industry-references.md](industry-references.md)，文件编码见 [product-format.md](product-format.md)。本文件锁定状态、所有权与失败规则，容量数值仍须实测调优。

## 1. Owner 与执行模型

| Owner | 执行位置 | 允许做的事 |
| --- | --- | --- |
| 调用方 | main/world/producer 或其他线程 | Try admission、Poll、Cancel、修改自己的 interest；不做隐含 open/stat/read/wait |
| Core IoScheduler | 一个控制线程 | 请求表、共享关系、优先级、配额、backend 派发与完成；不执行 plugin 解码或任意用户 callback |
| Core backend | 首版为少量专用 IO worker | open/stat/positional read；只写自己借用的 buffer，交回明确状态 |
| Framework AssetManager 协调器 | AssetExecutor 内一个串行控制线程 | cache/in-flight、metadata/dependency 状态机、准备计划、CPU ready 队列；不做阻塞 IO、重解码或编译 |
| CPU preparation | 现有 loader/cook CPU pool，经 bounded ready scheduler | 校验、解压、纯转换、允许的开发编译；不访问 live world/cache/device |
| 领域 owner | 既有 world/scene/renderer producer | 复核 ticket 与当前身份，并在同一次串行事务中发布 |
| Aurora | 既有 coordinator/uploader | GPU 上传、fence、Resident、pins 与退休 |

两个控制线程处理短状态转换，并非新增两个 CPU 工作池。元数据和 CPU 结果交接通过已预留容量的 mailbox；完成通知复用 Completion。异步资产的推进不依赖 main pump，因此同步工具等待 CpuReady 不会等自己被阻塞的 main continuation。公开 IAssetEvent 仍按现有主线程契约交付。

BackendCapabilities 固定包含：SupportsReadAt、内容能力 ImmutableVersion/Mutable、required buffer/offset alignment、maxTransferBytes、cancel 能力、execution mode、推荐并发。默认 Buffered Native/Memory 路径不要求 unbuffered/direct IO；未满足来源能力时返回 Unsupported。

第一版 PortablePoolBackend 的 workerCount 和 active backend slots 都从 2 起步，二者是不同配置。未来 native async backend 可用较少线程驱动更多 active ops；不改公共契约，不把设备 queue depth 等同于线程数。首版不引入 IOCP/io_uring/DirectStorage。

## 2. 最小公共操作

接口放既有 Core file/async target，以 PImpl/虚接口隐藏实现；下列是操作契约，不是可直接编译的头文件。

| 操作 | 输入 / 返回 | 契约 |
| --- | --- | --- |
| Init | 配置、拥有实际状态的 backend 实例 → Status | 校验全部容量，失败显式返回；不以构造器抛异常 |
| TryCreateScope | ClientId、root options → scope 或 admission error | Root admission 关闭后拒绝；scope/control 配额不足 Busy |
| TryRegisterWork | scope、已接受执行者的 registration lease → work lease | 只登记 generic 完成义务，不让 IO 池执行 CPU/领域 callback；实际 owner 在工作结束时完成 lease |
| TryOpenRead | scope、保活来源的 locator → open ticket | 实际 open/stat 在 backend；预留 retained-handle 名额，成功后名额随 handle 存活 |
| TryRead | scope、handle、u64 offset/size、interest options、可选 buffer reservation → read ticket | 服务持 owned backing；首版不支持无 ownership 的外部裸目标指针 |
| UpdateInterest | ticket、priority、steady-clock deadline | 只重排尚未 issue 的工作，不能抢占已进入 kernel 的 read |
| RequestCancel | ticket 或 scope | 请求取消，不阻塞，不代表底层结束；终态后无效 |
| Seal | scope | 关闭新的 root registration，已有 registration leases 可完成其已接受展开 |
| CloseRoots | service | 拒绝新 root；既有 scope 继续受预算推进，或由 host 显式 Cancel |
| Poll / TryGetResult | ticket → immutable state/result | Pending 没有成功 data；terminal 后数据/状态不变 |
| PollWorkDrained | service | 仅表示所有 accepted native/CPU/发布工作完成；通知排空另查 executor |

Admission 为 Accepted/Busy/Closed/InvalidArgument/Unsupported/CapacityExceeded；失败没有生成在途工作，也不消费调用方 ownership。Busy 指暂时资源被占；CapacityExceeded 指该单元或必要工作集根本超出支持上限，重试不会自行恢复。

运行错误至少区分 NotFound、AccessDenied、ShortRead、IOError、SourceChanged、InvalidData、Unsupported、DependencyCycle、CapacityExceeded、Cancelled。请求完成和成功分开；read result 提供 bytesRead 诊断，失败不返回可消费的半 buffer。generic work lease 必须显式 `Complete(Status)` 一次；重复完成返回 InvalidState，取消不代替实际 owner 完成。所有错误通过 Status/Result，不新增 throw/try/catch 或以 assert 代替恢复路径。

Try 操作可有短 mutex 临界区，不声称 wait-free。锁内不调用文件系统、handler、callback、native allocator，也不销毁可能运行 plugin 代码的最后一个引用。

## 3. 三类身份与共享

| 对象 | 身份 / 内容 |
| --- | --- |
| PhysicalRead | service session + operation serial；key 为 backend/handle identity + offset/size/read mode |
| PreparedBacking | immutable snapshot + block/blob selection + representation recipe/version + 完整 canonical 参数 + owned 数据 |
| PublicationTicket | 每 consumer 独有：scope interest、owner session、request serial、依赖 instance/revision；Aurora 再含 deviceEpoch |

共享 backing 不放某个 world/scene 独占的 epoch 或 request serial。两个世界可共享同一解码结果，但分别决定是否发布。representation 参数影响输出时必须进入 key；hash 只用于查表，完整 key 仍比较相等，不能因 hash 相同共享不同格式/agent/variant 结果。compound material/shader recipe 的 key 包含完整输入快照闭包：parent、overrides、shader index/artifacts 等；不能只在最后 ticket 检查依赖版本，而复用旧输入转换的内容。

共享层仅缓存 device-independent bytes、描述或 CPU recipe。现有 RenderResourcePayload 中含精确 resource/image keys 等 owner 状态，不能直接当作跨 deviceEpoch 的 PreparedBacking；由 producer 在复核 ticket 后包装精确资源版本。

Core 第一版只做 in-flight exact-range coalescing，不另建 raw file cache；Framework 对 immutable decoded block/backing 做有界 LRU，零 interest 且无外部引用的条目可驱逐。consumer quota 按自己的 logical interest/retained view 计数，全局物理 backing 只计一次；每个 join 仍占 control slot，不能借共享绕过配额。

共享操作的有效 priority 为 live interests 的最高值，deadline 为最早有效 deadline；queued Background 被 Critical join 时立即重排，取消 donor 后重新计算。准备任务和必要依赖也接受 donation，但不穿透已完成的不可变结果。

## 4. 调度算法与控制通道

1. Try admission 同时检查 scope gate、请求/control/handle 名额、byte credit 和 backend capability。成功后登记 ownership 再让 scheduler 可见；任何拒绝完整回滚预留。
2. scheduler 先收 native completions 和 cancel/close，再处理新 admission/ready CPU 消息。每个消息批次有上限，不能被 admission flood 永久压住完成回收。
3. 每个 backend 独立限制 active ops；同一个存储来源不因注册多个 ClientId 增加物理并发。来源同时报告 backend group，无法识别物理设备时使用保守共享组，不在 producer 探测磁盘拓扑。
4. 数据请求按 consumer weighted deficit round-robin；cost 为本次 native segment bytes，open/stat 使用固定控制 cost。每轮 quantum 有上限，禁止靠无限累计 deficit 霸占设备。
5. 选定可调度 consumer 后，按 effective priority、soft deadline、aging age、enqueue sequence 选择操作。aging 到阈值提升排队等级；同等级只有在不推迟更紧迫请求时才用同文件/相邻 offset 做 tie-break。
6. 默认 native segment quantum 为 256 KiB，最大 public read 为 4 MiB。大 read 可跨多个 segment；segment 间重排，首版每个 physical read 最多一个 active segment，避免一个大读占全部 slots。原生 maxTransfer 更小时进一步切分。
7. 每次 dispatch 先预留完成记录，再把作业放入 IO 专用 backend。CPU 同理经 TryDispatch；队列 Busy 保留 ready 节点，使用唤醒/容量通知重试，绝不在 worker 自旋等空位。

配置必须满足 `deficitCap >= 最大 native segment cost`；idle consumer 的 deficit 清零，重新活跃者从一个 quantum 起步，不能无限积累。priority donation/cancel 会更新该 physical operation 在所有 live sponsor 中的索引；使用可更新的有界队列节点，不以不断追加 stale heap entries 实现重排。

物理共享操作可从任一有 live interest 且有 deficit 的 consumer 获得 dispatch sponsor，只向该 sponsor 记本次服务 cost；全局实际 read bytes 只计一次。每-client logical-demand bytes 单独记录，便于解释共享带来的差异。

取消/关闭不依赖普通请求队列空位：每个 accepted ticket 预留 cancel/dirty 状态与 wake 能力，control 更新合并，不能每次更新在无界队列追加事件。完成 mailbox 的容量不少于已放行 backend/CPU jobs，接受操作时即保证最终通知有地方交回。超时只改变 interest/诊断，不提前释放 kernel 使用的内存。

## 5. 物理操作与 interest 状态

```text
PhysicalRead: Queued -> Issued -> Completing -> Terminal
                     \-> Terminal(Cancelled before issue)
Admission gate: Joinable -> ClosedToJoin   (never reopens)
Interest: Pending -> CancelRequested -> Terminal
                 \------------------> Terminal
```

- attach interest、last-interest cancel、关闭 join gate、移出 coalescing table 是同一串行事务。
- last cancel 胜出后，旧 physical operation 不可被新 join 复活；后来相同请求获得新 operation serial，旧 operation 仍保有自己的 buffer 和 active 配额直到 kernel 退出。
- join 胜出时新 interest 已登记，不能把它随原 last consumer 一起取消。
- consumer cancel 在该 consumer 终态 publication 之前线性化，则其结果为 Cancelled；之后的 cancel 不改写结果。其他 consumer 可成功，shared physical result 不被单个取消者改写。
- 对已 issue 的 shared read，取消 interest 的终态也等关联 native work 结束后发布，避免把该 token 当作提前回收屏障。
- 任一 segment 出错后停止未发 segment，收齐已发 segment；只有全部成功且精确读满才发布 data。短读循环处理到满足长度或明确 EOF/error，零进展 EOF 终结，不无限重试。
- token 析构既不自动取消也不证明 drain；service 仍收齐 accepted 请求。没有外部引用的最终结果随后正常释放，不永久留在请求表。

## 6. Scope 门禁与完成

Scope 区分 admission、工作完成、通知交付：

```text
Active -> Sealed -> WorkDrained
   \        \-> Cancelling -> WorkDrained
    \----------> Cancelling
```

RegisterChild 在同一 gate 下检查可注册状态、获取额度、增加 outstanding，再公开子任务。可能继续展开依赖的 parser/coordinator 持 registration lease；它必须先登记全部后续工作，再释放自己的 lease。不能在 outstanding 暂时归零后另起 dependency。

Cancel scope 与 RegisterChild 互斥：cancel 胜出后不允许新工作；注册胜出的工作计入 drain。Sealed 不允许外部新增 root，但既有 registration lease 可完成已接受的有界展开。

root admission 还预留其声明的有限展开 credit：nodes/edges/depth、control/metadata bytes、required handles。所有后续 child/open 从该 credit 划拨，不能先让 roots 占满 control/handles 后再让所有 parser Busy 等子项。超出 root 展开计划明确 CapacityExceeded 并终结 required parents；提高计划需重新 admission，不能在等待中无限长大。共享已有 node/handle 可降低实际占用，但首版允许保守重复预留；有外部读者的 handle credit 随对象存活，不能在 scope 终态提前归还。

WorkDrained 的必要条件：registration leases = 0，logical interests/CPU jobs/owner publication work 均已终结，所有关联 native writers 已退出。此时发布 scope Completion 一次。

**WorkDrained 不等于 callbacks drained。** Completion 发布后，已接受 continuation 才可执行；host 必须继续 pump 注册的 ContinuationExecutor 到 IsDrained，再释放相关 executor、callback 和领域数据 owner，DLL 代码仍由 loader 保持进程常驻。不能要求 callback 先执行再发布驱动它的 Completion。外部 retained buffers/handles 不阻止工作终态，但继续持 byte-accounting lease、backing 与所需 backend 实例状态。

## 7. 内存预留：可证明的进展条件

将参与 streaming 的 bulk/native 内存上限记为 H；对齐后的实际 allocations 为 A，未兑现的 escrow 为 R，始终满足：

```text
A + R <= H
P(unit) = input upper bound + decoded output + conversion/native output + scratch
```

第一版使用保守求和峰值，不依赖推测哪些对象一定不会重叠。**先在有界 metadata/control 阶段发现 required closure，再在任何成员开始 bulk 读取前原子预留整个 closure 的保守峰值**：`C = sum(P(required node/unit))`，另计不在这些 P 内的必需 pinned backing。unit 只是从 closure escrow 领取 input/output/scratch credit，不再逐级向全局抢额度。分配时从 R 转入 A；final backing 沿用其 allocation lease，结束时只归还未兑现 escrow，外部/GPU/retirement 仍持有的 A 不归还。

这一步是防跨 root 等待环的必要规则，单 unit 可完成还不够。H=64 MiB，A 依赖 B、X 依赖 Y，B/Y 各 20 MiB，A/X 各需另 30 MiB；若先完成 B/Y 就会占 40 MiB、两父都拿不到 30。closure escrow 让 A 或 X 先获得 50 MiB，另一 closure 不开始 bulk，因此至少已获 credit 的一方能完成。闭包所需最低工作集大于 H 明确 CapacityExceeded；暂时被外部已完成资源 pin 住则保持可观察的 WaitingBudget/Busy，不占 worker，caller 可取消或释放资源，不能声称在外部永不释放时仍有完成时限。

共享输入已有 backing 时全局 A 不重复记；第一版允许不同 closure 的未兑现 escrow 保守多预留，降低吞吐但不破坏 hard cap。共享 job 可从任一需要它且已取得 closure escrow 的 live scope 获得执行 sponsor，不能被最早但未获预算的 root 卡住。已借给 physical/CPU work 的 credit 必须活到真实工作退出或转为 backing lease；sponsor 取消不撤回正在使用的信用。优化 escrow 共享必须保证所有并行工作，不能重复许诺同一信用。

还必须满足：

1. metadata/control 有独立 capped headroom，可至少推进一个最大支持的 metadata parse；bulk 不能侵占它。
2. required dependencies ready 之前，父节点不分配 bulk/output/scratch，也不向全局单独申请 reservation；整闭包 escrow 已包含这份将来信用，成员按拓扑从中执行。
3. admission 证明整个 required closure 的 bytes、control 和 handle 工作集能放入 hard limits。准备过程中发现声明计划不足则明确失败，不继续持有半闭包等无法取得的额外 credit。
4. 非阻塞 ready planner 跳过暂时不可行 unit；可驱逐的 cache 优先归还；不让一个失败/oversize unit 永久挡队首。
5. 整坐标多 layer、完整 image gather 等不可分发布单元先预留收齐整个单元的峰值，不能各收一半后互相等剩余配额。
6. CPU worker 不等待预算，不等待自己 pool 的 future，不把已占输入留在 worker 栈上睡眠。

例：H=64 MiB，一个 unit 的 input/output/scratch 为 12/28/4 MiB，P=44 MiB；第二个 unit 可排队，但不能先读取额外输入占满余量再申请输出。若父 A 需保留 B 的 30 MiB 输出且自身 P=40 MiB，则 70>64，必须明确 CapacityExceeded，不能永久 Busy。

该上限覆盖参与协议的 backing/native/scratch，不声称限制整个进程 RSS。metadata、graph/control、retained handle 数另有限额；GPU staging/device memory 继续受 Aurora 自己的预算，若它复制 CPU bytes，该额外 CPU backing 也必须申领相应 credit。

## 8. 依赖、cache 与版本

Framework 协调器串行维护共享全局 readiness 图。`CurrentLoadKey` 为 UUID、target、mount/location epoch、load policy、registry 选定的 reload epoch；普通请求先按此 key 查找并 join 同一 `LoadOperationId`，不能把每 consumer 的 request serial 放进 lookup key。显式 reload/invalidation 才由 registry 推进 reload epoch，旧 operation 继续服务旧读者。ProductsOnly 不加入可能触发 cook 的 AllowCook 操作，只有已选定同一 immutable snapshot 的字节工作才可共享。

图节点为 `(selected LoadOperationId/instance identity, readiness phase)`，重复引用必须回到同一 selected operation，不能每条边生成新 identity 绕过环检测。Meta(A) 完成自身元数据校验即可就绪；Cpu(A) 可等待 Meta(B) 或 Cpu(B)。Reference 边只用于引用/打包，不是等待边。

required edge 插入与环检测是协调器的同一次事务，不能每个 root 各自 DFS 后并发接受反向边。环、node/edge/depth 超限立即失败并传播到 required parents；optional/reference 失败不伪装成 required 成功。元数据已成功和 CpuReady 失败是不同 phase 结果。

CpuReady 的 typed data 为独立描述/必要 CPU 数据，可含 BlobRef；它不自动拉取全部 mip/LOD/tile。producer 发布使用 PublicationTicket，身份检查和 cache/active 替换不可穿插 reload/override。旧结果进入 Stale 终态并释放自己的 interest，不影响其他 owner 的 backing。

长期打开的 metadata-only snapshots 同样消耗 handle/control 配额。cache 只可驱逐无活跃读者的 handle；不能关掉旧 handle 后靠重开 UUID 路径伪造快照。

## 9. 来源稳定性与平台边界

- ImmutableVersion：只允许引擎 checked writer 的临时文件+原子替换，或 immutable owned memory。旧 handle 稳定旧对象，expected metadata/block checks 保证读取属于被选择的产品。
- Mutable：只保证 positional read，不保证旧内容保存。原地写入可能造成 SourceChanged/InvalidData；大小/mtime/fileId 不是不可变性证明。editable shader source 必须经受控 cook/capture 形成有身份的 immutable artifact，不能把路径当内容版本。
- Windows 首版在 IO worker 使用明确 offset 的同步 native read，并在同 handle 上序列化共享 cursor 影响；完整 64-bit offset 与 short read checked。未来 OVERLAPPED backend 的 buffer/OVERLAPPED 必须活到真实 completion，CancelIoEx 返回不是 drain。
- POSIX 路径使用 checked pread，不依赖共享 seek cursor，处理 EINTR/short read；不支持的 package 来源返回 Unsupported，不偷偷读取整包模拟范围 IO。
- 默认使用 buffered IO。未来 backend 的额外 alignment padding、bounce bytes、over-read 都计入实际预算和 read amplification；不可满足时明确拒绝，不能越文件范围读取后静默丢弃错误。

## 10. Shutdown、保活与诊断

应用停止新领域需求 → CloseRoots → 对既有 scope 选择继续完成或 Cancel → 收齐 native/CPU/owner work → 发布各终态 → pump 已接受 continuations → 按既有 owner 退休 GPU/world → 关闭 consumer/backend 会话并释放已无人使用的实际对象；DLL 不卸载。已有 scope 的内部续接只有在未 Cancel 且有 registration lease/配额时合法。

Core backend、typed handler、CPU prepare、Ready 自定义析构、native retirement 都保活所需 backend/SDK 实例、服务及数据 owner；loader 负责代码的进程常驻，不逐层传递 module/code lease。external backing/handle 的 accounting state 自持，不指向已销毁 service。restart 使用新 session，旧 scope/ticket 不得向新会话提交；依赖 readiness 按会话状态检查，不从 DLL 已加载推断。资源热更新沿现有 revision/snapshot 与 CPU/GPU 退休协议进行，二进制更新、重载和移除须重启进程。

若底层驱动迟迟不返回，drain 只能报告 Pending/Stalled 与在途 operation/年龄，不能把超时伪装为完成、释放 buffer 或销毁仍被工作使用的 backend/SDK/服务对象。deadline 和取消不提供强制中止保证。

指标至少含：root/interest/physical op 数；queued/active/read/over-read bytes；reserved/allocated/retained bytes；open handles；metadata/CPU/owner/GPU ready 延迟；cancel requested 到 native terminal；通知排空年龄；failure/retry/CapacityExceeded。trace 用 root→ticket→physical-op→backing→publication 的关联 id，而非只记 UUID。

## 11. 必须通过的反例

| 场景 | 验收断言 |
| --- | --- |
| Critical 加入 queued Background 共享读 | 重新排序并捐赠优先级；取消 donor 后恢复其余 interests 的优先级 |
| 最后 cancel 与新 join 同时发生 | join 或 close 只有一个先后次序；不复活旧操作、不丢终态、不复用在写 buffer |
| parser 在 child 注册前被 cancel | cancel 后不产生新 dependency；注册先发生的工作被 drain；root 不提前完成 |
| CPU 队列在依赖完成时已满 | completing worker 不自旋；ready 节点仍有 owner，稍后激活 |
| 并发 root 形成 A→B、B→A | 同一共享图拒绝环并传播明确失败 |
| parent 等 dependency、两者单独能放下 | parent 不抢 bulk escrow；真实 closure 超限明确失败 |
| 两 root 先准备各自 dependency 会占满 H | closure escrow 在 bulk 前取得；不能形成 B/Y 各 pin 一半、两父均等待的状态 |
| roots/父 snapshots 占满 control/handle | root 预留有限展开 credit；子项超计划明确失败，不无限 Busy |
| 多坐标各有多个 layer、Ready 空间很小 | 不形成全部只完成半坐标的永久 Pending |
| A/B 共享解码，A detach | A ticket stale/cancelled，B 可发布；只做一次兼容 read/decode |
| 产品原子替换 / 原地 overwrite | 旧 immutable handle 读旧对象；mutable 变更不获得旧 snapshot 保证 |
| 零 bulk metadata snapshots 不断增加 | handle/control 限额生效，不能耗尽系统 descriptors |
| Close 后旧 parser 继续展开 / restart | 合法 scope 受限继续或显式取消；旧 session 不进入新队列 |
| kernel 结束但通知未执行 | WorkDrained 与 executor IsDrained 分开，module 仍保活 |
| GPU upload 保留 CPU view | read slot 可归还，byte lease 不归还，现有 GPU fence 决定 Resident |
