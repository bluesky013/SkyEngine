## MODIFIED Requirements

### Requirement: RenderGraph 三段式 lifecycle

RDG SHALL 保留 Setup -> Compile -> Execute 的语义阶段，具体入口为公共声明图 Freeze、对应后端完整 Compile、对应 DeviceFrameContext Dispatch/收齐编码结果；Queue Submit SHALL 为显式后续步骤。公共层 SHALL 提供 IR、诊断和可复用纯分析，不固定所有后端的 native pass、编码任务或 command buffer 数量。Compile SHALL 不执行 acquire、present、队列提交或编码 jobs。

#### Scenario: 由活动后端编译
- **WHEN** 同一份合法声明图分别交给 Vulkan、DX12、Metal 的 FrameContext
- **THEN** 分别由各自后端生成匹配的 compiled plan，满足相同逻辑语义但允许不同 native scopes、编码任务和命令组合

#### Scenario: dispatch 只读编译产物
- **WHEN** 编译结果进入 Dispatch
- **THEN** 编码任务只读其已冻结 payload/bindings 与显式前置任务发布的快照，不读取可变 setup graph、Scene ECS 或宿主对象

#### Scenario: 帧末状态转换
- **WHEN** 输出声明了最终状态
- **THEN** 后端在消费者完成后的合法 native 边界发出对应同步，候选最终状态仅在提交接受后进入账本

### Requirement: Resource declaration: Import vs Create

RenderGraph SHALL 区分引用计数外部资源、borrowed swapchain output 与 transient 资源。普通 ImagePtr/BufferPtr 导入 SHALL 保留引用；swapchain image SHALL 由 swapchain 唯一持有，通过 borrowed lease 导入，不得构造会删除该 image 的 CounterPtr。CreateTexture/CreateBuffer SHALL 使用 transient pool。资源名称 SHALL 使用 core::Name。提交使用的资源引用/leases SHALL 转交 FrameSlot 保活至完成，不能仅依赖 CPU graph 的生命周期。

#### Scenario: Import 外部引用计数资源
- **WHEN** 调用方导入普通 ImagePtr 后释放自身引用并结束 CPU graph 生命周期
- **THEN** 若 GPU 提交仍使用该资源，FrameSlot 的引用保持其存活至提交完成

#### Scenario: Import swapchain output
- **WHEN** 调度器导入已获取的 swapchain output lease
- **THEN** graph 能引用其 Image，且 graph 销毁不释放 swapchain 所拥有的 image

#### Scenario: CreateTexture 命中可用池资源
- **WHEN** 相同描述的池资源没有未完成提交占用
- **THEN** 新 transient 分配可复用它，调试统计显示 hit

### Requirement: Compile 推导 barrier 并自动 emit

各后端 Compile SHALL 按物理资源的 GPU 使用顺序和 native pass scope 推导合法同步，由 FrameContext Dispatch 的编码计划自动 emit；同步可为 barrier 或满足该后端要求的 encoder 边界/其它原语。pass 回调 SHALL NOT 自行插入未声明的资源 barrier。导入资源 SHALL 声明实际初始状态及所需最终状态；新建且允许丢弃内容的资源可从 UNDEFINED 开始，跨帧持久图像不得无条件假定 UNDEFINED。重复导入同一物理资源 SHALL 统一身份与状态；同帧物理复用 SHALL 包含跨逻辑 handle 的同步。export 的最终状态 SHALL 按声明处理，窗口输出为 PRESENT。CPU 任务依赖 SHALL 不被当作 GPU 内存同步。

#### Scenario: 持久离屏图像跨帧使用
- **WHEN** 上一帧最终为 shader-read 的持久图像在下一帧重新作为 color attachment
- **THEN** 产生从已记录状态到 color attachment 的转换，并在本帧结束后保存新的最终状态

#### Scenario: 连续只读与重复写读
- **WHEN** 图内同一资源依次写、读、读、再写、再读
- **THEN** 保持所有必要依赖，连续相同只读不重复转换，后一次写读不会错误连接到更早的 writer

### Requirement: Pass culling 自动剔除未消费 pass

Compile SHALL 从有效导出输出、显式 MarkOfInterest、获准持久上传和其它明确副作用根反向保留必要内容依赖；普通 Import 本身 SHALL 不自动成为活跃根，失效 viewport 输出不得成为活跃根。未消费的 transient 写入 SHALL 被剔除且不分配 backing。依赖不可用资源的 pass SHALL 不执行并报告原因；仍有有效 consumer 的共享生产者 SHALL 保留。调试信息 SHALL 列出 live/culled pass 与原因。

#### Scenario: 写未读的 transient pass 被剔除
- **WHEN** Pass A 写 transient T，T 无后续消费者且未标记为输出
- **THEN** A 不执行，T 不分配实际 image

#### Scenario: 一个窗口输出失效
- **WHEN** 某窗口输出不可用，但同一 scene 的另一输出仍可用
- **THEN** 仅保留有效输出所需的依赖，失效分支不产生 Present 或访问空 backing

### Requirement: Transient pool 复用底层 RHI 资源

RDG SHALL 使用 device 级 transient pool，按完整兼容描述（含尺寸、格式、samples、usage）与使用区间选择 backing。不同未完成提交 SHALL 通过 frame leases 或 slot 分区隔离。单图内不重叠逻辑资源可复用同一 backing，但 SHALL 保留其物理访问同步。淘汰 SHALL 只释放无在途引用的资源，不得仅按 CPU 帧数判断安全。

#### Scenario: 同帧内不重叠 transient 复用
- **WHEN** 多个兼容 texture 的使用区间不重叠且允许丢弃旧内容
- **THEN** 可以复用同一 Image，并包含前后物理使用间必要 barrier

#### Scenario: 跨帧稳定复用
- **WHEN** inflight ring 预热后持续使用相同描述且对应提交已完成
- **THEN** 后续帧复用空闲 backing；GPU 未完成的 backing 不被提前分配给另一提交

### Requirement: CompiledPass variant payload

公共 logical payload SHALL 保持 SceneRaster、FullScreen、Compute、CopyBlit、Present、Custom 的类型语义，供后端降为自己的 native pass/encode plans，不要求 native 产物沿用统一 variant 或 flat pass 列表。编译产物 SHALL 保存每个 logical pass 解析后的有效 set 0 binding；整图 globalResourceGroup 仅可作为显式 fallback。SceneRaster payload SHALL 保存当帧收集的 DrawItems，编码任务 SHALL 只读已发布的 compiled 数据。

#### Scenario: SceneRaster payload
- **WHEN** 带有 DrawItems 和 View binding 的 SceneRaster pass 被编译
- **THEN** payload 保留这些 items 及该 pass 的有效绑定，不从共享可变 View 成员重新读取

#### Scenario: native pass lowering
- **WHEN** 后端将多个 logical passes 合并或将一个 pass 拆成多个编码单元
- **THEN** 保留访问、绑定与绘制顺序语义及调试映射，不要求公共 executor 按同一 switch 遍历

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

AddSceneRasterPass SHALL 只接收 setup，execute SHALL 由 DrawItems 驱动。DrawItem SHALL 携带 PSO、几何 streams、index type/offset、draw args、batch RG 和 dynamic offset。executor SHALL 先绑定 Pipeline，再绑定兼容的 set 0/1/2；切换 layout 后 SHALL 重新绑定所需 sets。batch RG 非空时 SHALL 应用 batchDynamicOffset。几何绑定 SHALL 保留实际 vertex streams、offsets 和 U16/U32 index type，不得硬编码单 stream 或 U32。

#### Scenario: SceneRaster items 驱动绘制
- **WHEN** 收集的 item 具有有效 PSO 与 batch RG
- **THEN** Pipeline 先于所需 ResourceGroups 绑定，应用 dynamic offset 后发出该 item 的绘制

#### Scenario: 多 stream 与 U16 index
- **WHEN** 几何使用多个 vertex streams 与 U16 索引
- **THEN** executor 按其实际布局绑定所有 streams 和索引类型并 DrawIndexed

#### Scenario: shader 生成顶点
- **WHEN** program 的 vertex input 为空，item 使用 shader 生成顶点且 draw args 有效
- **THEN** 可以不绑定 vertex buffer；缺少必需 vertex input 的其它 item 应报告无效并跳过

### Requirement: Global ResourceGroup 生效

RenderGraph SHALL 提供 graph default set 0 和逐 pass override。Compile SHALL 为每个 raster/fullscreen/compute pass 解析有效 set 0，Execute SHALL 在 Pipeline 绑定之后绑定所需 RG。不同 View/FrameSlot 的可变常量和 descriptor SHALL 独立，未完成 slot 不得覆写；该规则 SHALL 同时适用于 scene、fullscreen 和 compute payload。

#### Scenario: global RG fallback
- **WHEN** graph 设置 default RG 而 pass 没有 override
- **THEN** pass 使用 default RG，在相应 Pipeline 已绑定后绑定 set 0

#### Scenario: 双 View 常量隔离
- **WHEN** 同一 graph 的两个 pass 分别声明 View A 和 View B 的 set 0
- **THEN** 各自使用对应相机常量，后建图的 View 不覆盖先建图的 View

### Requirement: BindViewport 资源

RDG SHALL 导入调度器已 acquire 的 viewport output lease，携带 image、extent、format、generation 和状态；BindViewport 若保留为兼容包装，SHALL 只消费已获取结果。Compile SHALL NOT 调用 Acquire 或 Present。只有有效输出可成为 culling 根；失效 lease SHALL 被拒绝，相关分支不得执行。

#### Scenario: BindViewport 返回可用 handle
- **WHEN** 调度器已 acquire 成功并导入有效 lease
- **THEN** 得到可用于 ColorAttachment 和 Present 声明的 handle，使用实际输出 extent/format

#### Scenario: backbuffer 是 culling 种子
- **WHEN** 有效 backbuffer 的 writer 没有其它 consumer
- **THEN** writer 仍为 live，以完成该输出

#### Scenario: 重复 Compile
- **WHEN** 同一 setup graph 使用同一有效 output lease 再次编译
- **THEN** 不发生第二次 acquire，不切换 image index

## ADDED Requirements

### Requirement: Descriptor resolve 与访问声明

setup SHALL 声明 RG 中图资源的访问和 binding 意图；后端 Compile 解析 backing 后 SHALL resolve descriptor，或为不能提前完成的部分声明显式 CPU 前置任务。消费任务执行前 SHALL 发布不可变 binding 快照。经 descriptor 使用的纹理/缓冲 SHALL 同样声明 RDG Read/Write。最终 bindings SHALL 不被并发编码任务修改，并由 FrameSlot 保活至提交完成。

#### Scenario: transient HDR 被合成采样
- **WHEN** ScenePass 写 transient HDR，合成 pass 声明 SRV 读取
- **THEN** descriptor 指向 Compile 解析的同一 backing，图包含写到采样的 barrier，Execute 时无需访问 setup graph

### Requirement: 读取版本与内容依赖

RDG SHALL 将每次 Read 绑定到其逻辑顺序前的生产版本，区分保留旧内容的 ReadWrite 和有完整覆盖证明的 WriteDiscard。attachment LOAD/部分写及其它保留内容操作 SHALL 追溯必要旧 producer；STORE_DONT_CARE 和未初始化范围不得被后续读取当作有效内容。Culling SHALL 沿内容依赖进行，之后按存活访问重建 RAW/WAR/WAW 和显式执行顺序，不能用资源的最终 lastWriter 代替读取来源。

#### Scenario: 中间读取与后续覆盖
- **WHEN** A 写 R，B 读 R 并产生导出 O，C 完整覆写 R 且无 consumer，只有 O 是根
- **THEN** A/B 保留，C 可剔除；B 读取 A 版本；若 R 的最终版本也被导出，则 C 保留且 B 在 C 覆写前读取

#### Scenario: UI LOAD 保留底图
- **WHEN** Scene 写 color，UI pass 使用 LOAD 在同一 color 上叠加并导出
- **THEN** UI 的内容依赖保留 Scene，不能只保留最后一个 UI writer

### Requirement: Typed CopyBlit 实际执行

CopyBlit SHALL 为 buffer copy 与 buffer-to-image upload 提供类型正确的 source/destination、拥有数据的 region 描述和真实 encoder emit。offset、pitch、mip、layer、extent 与后端对齐/格式约束 SHALL 被验证。其它公开 copy kind SHALL 有实现或显式 Unsupported，不得静默无操作。staging 与目标资源 SHALL 声明对应 RDG read/write 并由 slot lease 保活。

#### Scenario: Vertex index 与 texture 首次上传
- **WHEN** 图内包含 staging 到 vertex/index buffer，以及 staging 到指定 texture mip/layer 的 copy
- **THEN** compiled payload 不将 buffer 误解析成 image，执行真实 copy，后续 draw/readback 观察到预期数据并具备正确 barrier

### Requirement: 编译结果与提交状态发布分离

Compile SHALL 返回可检查的成功或诊断结果，不执行 acquire/present/submit。资源最终状态、upload receipts 与 GPU leases SHALL 作为候选提交产物输出；只有 Accepted submission SHALL 推进队列顺序状态账本，成功完成才推进 completed serial。重复 Compile 或丢弃未提交图 SHALL 不发布上传完成或资源全局状态。

#### Scenario: 编译两次后放弃
- **WHEN** 同一 graph 被重复 Compile，随后在 Submit 前放弃
- **THEN** 不额外 acquire，不产生成功上传通知，不推进资源的已提交最终状态，候选 leases 按未提交路径释放

### Requirement: 后端完整编译与 native pass 构建

每个后端 SHALL 自行组织完整 Compile，涵盖内容分析/剔除、GPU 顺序、native pass scope 构建、lifetime/backing、bindings、同步、EncodeTaskGraph 和 SubmissionPlan。公共纯算法 MAY 复用，但后端接口 SHALL 不限于 CompileBarriers/公共 ExecutePasses 转调。合并/拆分 SHALL 保持 attachment LOAD/STORE/resolve、内容定义范围、顺序敏感 draws 和 Custom 的受限行为；Unsupported SHALL 明确报告。GPU 顺序变化后 SHALL 重新验证 lifetime/alias/同步。

#### Scenario: 合并 scope 影响同步
- **WHEN** 合并两个 raster passes 导致所需 barrier 不能放在合法位置
- **THEN** 后端拆开 scope 或选择经验证的合法同步方案，并重新验证资源计划，不静默删除 barrier

#### Scenario: 跨后端一致的内容剔除
- **WHEN** 图包含中间读、后续无用覆盖以及 UI LOAD
- **THEN** 各后端保留相同必要内容语义，可产生不同 native plans，不能因优化读取未定义内容

### Requirement: CPU EncodeTaskGraph 与 GPU 执行计划分离

后端 SHALL 分别表达 GPU 访问顺序/同步和 CPU 编码任务依赖。编码 DAG SHALL 只添加 CPU 数据发布、native encoder 生命周期、独占录制资源及结果组装所需的边；GPU producer/consumer 依赖 SHALL 不自动变为 CPU 串行边。每个任务 SHALL 有稳定结果身份和 executor 约束。SubmissionPlan SHALL 按确定的 GPU 顺序组装结果，不能按 worker 完成次序提交。未声明可并行的 Custom callback SHALL 使用 coordinator 串行路径。

#### Scenario: 上传与消费者同时编码
- **WHEN** upload 与 draw 需要的 backing/bindings/CPU packets 已冻结且 native 录制互相独立
- **THEN** CPU 可同时编码两者，GPU 仍执行 copy 到 draw 的必要顺序和同步

#### Scenario: 顺序敏感 draw 的分块
- **WHEN** 同一有序 draw 队列的连续片段在不同 worker 编码且后段先完成
- **THEN** native 命令仍按原片段顺序组装；拆分不会重复 clear 或丢失前段结果

### Requirement: 后端编译计划身份与单次消费

BackendCompiledFrame SHALL 绑定 backend、device epoch、context identity、slot generation、GraphId 与 PlanId，由其后端管理 native payload。Dispatch SHALL 校验匹配并单次消费计划，保活计划/模块及资源到任务 drain。RecordedFrame SHALL 仅在全部任务退出且组装成功后产出，携带实际编码 receipts；仅匹配的 context 可将其单次提交。编译失败、取消和重复调用 SHALL 不推进全局资源状态。

#### Scenario: 错 context 或过期计划
- **WHEN** plan 被交给其它 context、旧 slot generation 或第二次 Dispatch
- **THEN** 返回可诊断错误，不执行 native 任务或修改当前槽位

#### Scenario: 部分任务失败
- **WHEN** 一部分 native 任务已录制，另一个任务失败
- **THEN** 收齐所有已启动任务、收尾编码对象，不产出可提交 RecordedFrame；GPU 与输出租约按各自未提交协议处理

## REMOVED Requirements

### Requirement: Execute 输出到单一 CommandBuffer

**Reason**: 各后端拥有 native pass 与并行编码计划，单一外部 CommandBuffer 无法表达 DX12 多 DIRECT lists 或 Metal parallel encoder 的生命周期。

**Migration**: 使用匹配的 DeviceFrameContext Compile -> Dispatch -> TryCollect -> Submit；直接 RHI 工具路径仍可手动编码，但不能绕过 RDG EncodingSession 的 slot 所有权。
