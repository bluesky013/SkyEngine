This delta defines the final public contracts of `refactor-aurora-framegraph-contracts`. For the requirements named below it supersedes earlier API wording, including overlapping active change deltas; unmodified shader semantics, target ABI, supported capabilities and retirement requirements remain in force.

## MODIFIED Requirements

### Requirement: Compile 推导 barrier 并自动 emit

各后端 Compile SHALL 按物理资源的 GPU 使用顺序和 native pass scope 推导合法同步，由 FrameContext Dispatch 的编码计划自动 emit；同步可为 barrier 或满足该后端要求的 encoder 边界/其它原语。pass 回调 SHALL NOT 自行插入未声明的资源 barrier。导入资源 SHALL 声明实际初始状态及所需最终状态；新建且允许丢弃内容的资源可从 UNDEFINED 开始，跨帧持久图像不得无条件假定 UNDEFINED。重复导入同一物理资源 SHALL 统一身份与状态；同帧物理复用 SHALL 包含跨逻辑 handle 的同步。export 的最终状态 SHALL 按声明处理，窗口输出为 PRESENT。CPU 任务依赖 SHALL 不被当作 GPU 内存同步。

Compile SHALL 从同一 typed 声明遍历取得临时 resource-use facts，保留有效 buffer 范围、image view/subresource 范围、usage role、stage 和 access。内容 reads/writes/defines SHALL 保持独立；公共 facts SHALL NOT 固定 native barrier 或转成持久第二份 Frozen IR。同一物理 buffer 在同 pass 的 vertex/index/uniform/storage 只读角色 SHALL 在 native lowering 前合并，不能漏掉 raster storage 读取或用后一次角色覆盖前一个角色。

#### Scenario: 持久离屏图像跨帧使用
- **WHEN** 上一帧最终为 shader-read 的持久图像在下一帧重新作为 color attachment
- **THEN** 产生从已记录状态到 color attachment 的转换，并在本帧结束后保存新的最终状态

#### Scenario: 连续只读与重复写读
- **WHEN** 图内同一资源依次写、读、读、再写、再读
- **THEN** 保持所有必要依赖，连续相同只读不重复转换，后一次写读不会错误连接到更早的 writer

#### Scenario: 深度 resolve 后续深度读取
- **WHEN** 固定功能 depth resolve 写入单 sample 图像，后续将其 LOAD 或跨图继续使用
- **THEN** 同步覆盖真实 resolve 写阶段/访问及后续 depth 读取；Vulkan 不因 attachment layout 为深度格式而漏掉 COLOR_ATTACHMENT_OUTPUT/COLOR_ATTACHMENT_WRITE，使用实际子资源范围

#### Scenario: 不同子资源交错访问
- **WHEN** 图内交错 copy/read 多个 mip/layer，或多个 sampled views 范围重叠
- **THEN** 各后端按物理 backing 的实际子资源旧状态推导同步，copy/view 只转换访问范围，重叠只读不重复转换；alias 保留物理状态且不继承逻辑内容

#### Scenario: 混合图内状态汇合到统一边界
- **WHEN** 图内多个子资源处于不同状态，另一些未被访问，但 image 声明统一 final layout
- **THEN** 后端从各实际旧状态完成所需最终转换，不假定全 image 仍处于最后一次访问的状态；下一图以该统一边界和实际有效内容导入，失败/取消不提交状态更新

#### Scenario: 混合图边界与无 pass 导出
- **WHEN** 各 mip/layer 有不同 initial/final，或只有 image export 而没有存活 pass
- **THEN** 各 backend 从每个实际旧状态生成对应最终转换，包括未触及 cell；Vulkan 按同层连续相同旧/新状态合并，DX12 不用 ALL_SUBRESOURCES 覆盖混合状态。图声明与原生同步分离，两个 Vulkan compiler 仍输出独立 encoder

#### Scenario: Storage 与 vertex 同时读取
- **WHEN** copy/compute 写入的同一 buffer 随后同时用作 raster vertex 输入和只读 storage
- **THEN** 分析保留实际读取范围及 producer，native 同步覆盖两类读取 stage/access，不只同步几何或仅使用最后一次状态

### Requirement: 冻结图的类型化 pass payload

冻结图 SHALL 持有 BufferCopyPass、BufferImageCopyPass、FrameRasterPass、FrameComputePass 与 FrameCustomPass 的类型化声明；fullscreen 使用生成顶点的 FrameRasterPass，presentation 由输出调度器管理。后端 SHALL 生成自己的 native pass/encode plans，保留每个 draw/pass 的实际绑定和输入所有权；不提供旧 CompiledGraph 或可变 setup graph 的执行旁路。

RasterDraw 和 FrameComputePass SHALL 仅保存按真实 set 组织的 immutable FrameBindingGroup 引用和独立 per-use overrides，不保留原先 flat binding arrays 作为第二事实源。Group SHALL 持有 shader-derived layout owner、真实 set、精确 graph handles/base ranges/views 和 sampler 所有权；buffer/image access 与 dynamicOffset SHALL 属于 use，不进入共享 descriptor value identity。临时 RasterBindingDraft/ComputeBindingDraft SHALL 在显式 Set 成功后整体发布组；失败 SHALL 保留原有 groups。

Group 构造 SHALL 规范化 binding 顺序并保持数据不可变；ValidateBindingGroups SHALL 拒绝 owner/set 不符、重复 binding、unknown/重复 overrides 或没有 snapshot 的 overrides。Custom SHALL 保留原 ordered 声明数组及回调资源顺序，不能为统一分组改变其契约。

#### Scenario: Raster payload
- **WHEN** 一个包含 RasterDraw 与显式资源绑定的 FrameRasterPass 被编译
- **THEN** native plan 保留对应输入快照，不从共享可变 View 或旧 DrawItem 列表重新读取

#### Scenario: native pass lowering
- **WHEN** 后端将多个 logical passes 合并或将一个 pass 拆成多个编码单元
- **THEN** 保留访问、绑定与绘制顺序语义及调试映射，不要求公共 executor 按同一 switch 遍历

#### Scenario: 公开 pass kind 的实际行为
- **WHEN** 图包含 Compute、FullScreen 或 Custom 等公开 pass kind
- **THEN** 受支持的 kind 生成并执行对应 native 命令，不支持者明确返回 Unsupported；Custom 只使用声明访问和获准 executor，不通过空分支伪报成功

#### Scenario: raster attachment 显式单 mip/layer
- **WHEN** RGBA8 color 与可选 D32 depth 从单 sample 2D image 中分别选择 mip/layer
- **THEN** 默认 view 选择 0/0，越界或选中 extent 不匹配显式失败；允许 base extent 不同而所选 mip extent 相同，viewport/scissor 使用选中 extent，原生 view/descriptors 不包含其它 cell

#### Scenario: attachment 内容只影响选中子资源
- **WHEN** pass 对选中的 cell 执行 LOAD、CLEAR 或 STORE_DONT_CARE
- **THEN** LOAD 只读取该 cell 的已定义内容，CLEAR/STORE 只定义该 cell，丢弃只失效该 cell；相邻 cell 和此前读版本保持独立，整图 export 仍要求全部内容，同 pass 采样 attachment 仍按整 image 拒绝

#### Scenario: MSAA color 的显式 resolve 目标
- **WHEN** 支持的 2/4/8× RGBA8 color pass 声明单采样 resolve image/view
- **THEN** 源为单 mip 2D render-target storage，目标是不同 image、同格式/所选 extent 的单采样 render target；源通过 CLEAR/LOAD 定义全部 samples，resolve 只定义目标 cell，源 STORE_DONT_CARE 不影响 resolve 结果，源后续 LOAD 仍要求源内容已存储

#### Scenario: resolve 范围与未支持声明
- **WHEN** 图声明越界 view、不同格式/extent、单采样源、多采样目标、DONT_CARE 源，或同 pass 采样 resolve target
- **THEN** 编译拒绝；未支持的 depth/stencil resolve 模式、多采样局部初始化、buffer-image copy 和 sampled binding 未接入时明确 Unsupported；resolve 不扩大整图 export 或其它 cells 的内容资格，死 resolve 随 pass 裁剪

#### Scenario: 多采样深度附件与 color resolve 组合
- **WHEN** raster pass 使用设备支持的 X2/X4/X8 RGBA8 color 与 D32 depth，可选 color resolve
- **THEN** depth 为单 mip、GPU_ONLY、DEPTH_STENCIL storage，选中 extent/sample count 必须匹配 color；不匹配、越界和未定义 LOAD 在 transient 分配前拒绝。深度 CLEAR/STORE 定义选中 cell 的全部 samples，丢弃后不可 LOAD，color resolve 不定义 depth 或其它 cells

#### Scenario: 多采样深度串并行与原生 scope
- **WHEN** 连续 passes 保留深度内容并跨多个 encode fragments 绘制，期间也可能写入其它 layer
- **THEN** 串并行产生相同的深度遮挡结果；Vulkan RenderPass 正确关联 color/depth/resolve 三附件，无 resolve 时保留相容多 subpass 合并；Dynamic Rendering 使用匹配的 depth view/PSO/inheritance；DX12 使用匹配 samples/array slice 的 DSV，color resolve 在全部 draws 后执行，其它 layer 内容不被误改

#### Scenario: D32 depth resolve 模式由调用方显式选择
- **WHEN** raster depth attachment 声明 resolve 目标与 MINIMUM 或 MAXIMUM
- **THEN** source 为支持的 D32 多采样 storage，目标为不同 backing 的 GPU_ONLY 单采样 D32 depth attachment，所选 extent 相同；mode 只表示数值 min/max，不按 compare/clear 推断，未指定或未知 mode 拒绝。设备不支持所选模式返回 Unsupported，不替换模式或隐式 shader 模拟

#### Scenario: depth resolve 内容与源存储独立
- **WHEN** source depth 由 CLEAR 或合法 LOAD 保证全部 samples 有效，pass 结束时 resolve
- **THEN** 只定义目标选中 cell，source STORE_DONT_CARE 不影响结果；源丢弃后的读取及未写目标 cell 的读取仍拒绝。后续单采样 raster 可 LOAD 目标；只消费目标也保留原 raster，死 pass 可裁剪，重复 resolve 的中间版本、物理身份和同 pass alias 限制不得丢失

#### Scenario: depth resolve 的可观察模式与三套原生实现
- **WHEN** 部分 source samples 的 depth 不同，后续单采样 raster 读取 resolve 结果进行深度比较
- **THEN** MINIMUM/MAXIMUM 产生预期的不同结果，串并行一致；Vulkan RenderPass 使用 depth-resolve subpass 扩展与正确 attachment index，Dynamic Rendering 使用 depth resolve attachment；DX12 在最后 draw fragment 后执行对应 resolve task；同时 color/depth resolve 及非零目标 mip/layer 不错配

#### Scenario: Immutable groups 与不同动态窗口
- **WHEN** 多个 raster/compute 使用相同布局与资源值，但具有不同 access 或 dynamic offsets
- **THEN** 共享 group 仍不可变，各 use 解析自己的有效范围/访问；不同 shader/layout owner、base range、graph resource 或 view 不得误认为同一 group

#### Scenario: Group 发布失败
- **WHEN** Set 收到非法 set/binding 或重复资源槽，或者手工 group/use 包含 unknown override
- **THEN** Set 保留旧组，编译前置验证拒绝非法 group/use，不静默丢弃 overrides 或改写 frozen 值

### Requirement: Raster pass execute 由冻结 draw 驱动

FrameRasterPass SHALL 通过 RasterDraw 声明 shader、几何 streams、index type/offset、draw args 与 set/binding 资源。后端 SHALL 在 Compile 生成匹配的 pipeline 和不可变 bindings；编码时先绑定 Pipeline，再绑定实际活跃的 set 0/1/2/3（Global/Pass/Material/Batch）。切换 layout 后 SHALL 重新绑定所需 sets。动态偏移 SHALL 来自对应声明；几何绑定 SHALL 保留实际 vertex streams、offsets 和 U16/U32 index type，不得硬编码单 stream 或 U32。

结构与 reflection/binding 匹配 SHALL 在 pass culling 前验证；只有存活 pass 和 exports 才要求内容已定义。后端 materializer SHALL 在当前 Compile/lease 中按 immutable group 值和已验证 layout 复用 native bindings，dynamic offsets 每次 use 独立生成；不能以整个 draw 或 dynamic offset 作为共享 Global/Pass/Material descriptor 的失效条件。不同实际 backing/version/base range/view/layout SHALL 不兼容，GPU pins 仍按提交退休。

Metal SHALL 在原始 raster render pass 内同时降低 attachment preservation 和 resolve：source STORE 时使用 store-and-resolve 语义，source DONT_CARE 时仍先产生 resolve 目标；SHALL NOT 先丢弃 source 再通过额外 LOAD pass 假装完成原 resolve。

#### Scenario: Raster draws 驱动绘制
- **WHEN** 冻结 draw 具有有效 shader 与 Batch bindings
- **THEN** Pipeline 先于所需 ResourceGroups 绑定，应用 dynamic offset 后发出该 item 的绘制

#### Scenario: 多 stream 与 U16 index
- **WHEN** 几何使用多个 vertex streams 与 U16 索引
- **THEN** executor 按其实际布局绑定所有 streams 和索引类型并 DrawIndexed

#### Scenario: 几何与常量属于图读取依赖
- **WHEN** indexed item 使用图中刚上传的 vertex/index 范围与带 dynamic offset 的 UBO 范围
- **THEN** 内容分析保留相应 producer，验证最终读取范围并生成 copy 到 raster 的同步；缺失 producer、越界、错误 usage 或不兼容 reflection 显式失败

#### Scenario: 完整 mip 链纹理采样
- **WHEN** raster draw 声明 RGBA8 Texture2D image 和独立 sampler，纹理由本图上传或先前 Resident 版本提供
- **THEN** 采样读取纳入内容依赖与存活区间，全部暴露 mip 必须已定义；保留 producer 并生成 VS/FS 采样所需同步，拒绝同 pass attachment feedback

#### Scenario: 显式 mip/layer view 与部分内容读取
- **WHEN** 2D/2D-array sampled view 只暴露 image 的一部分 mip/layer，而未暴露子资源没有内容
- **THEN** 图只为暴露范围建立读取依赖并保留其 producer，缺失任一所选内容或越界范围失败；全图 export 仍要求全部内容有效，view 不扩大 initialized 承诺

#### Scenario: shader 资源形态前置校验
- **WHEN** shader 声明 descriptor 数组、comparison/MSAA/storage 或当前 raster 路径不支持的 texture 类型，或者 view 形态与 shader 不匹配
- **THEN** adaptor 在创建 native shader 前拒绝不支持声明，Compile 复核绑定与 view；长度一数组和未知反射信息不得作为受支持 scalar 资源，cache 缺失类型元数据时拒绝加载

#### Scenario: array view 的相对坐标与片段一致性
- **WHEN** 多个编码片段采样不同 baseMip/baseLayer 的 array views，或只暴露一个 mip/layer
- **THEN** LOD/layer 相对各自 view，native SRV/view 范围一致，串行与并行读回一致；array image 的单层切片仍要求 array view 与对应 shader 类型

#### Scenario: sampled view 创建失败与提交退休
- **WHEN** Compile 在部分 native view 创建后失败，或已冻结 view 经取消、不确定提交或完成查询错误退出
- **THEN** 失败清理已创建的未提交对象并允许安全重试；正常编码不创建/修改 view，所有在途 view 与 backing 保活至成功完成或安全 teardown，取消通知和错误回执不提前退休

#### Scenario: 采样 descriptor 冻结与预算失败
- **WHEN** 多个 worker 编码不同 image/sampler/layout 的 draws，或者某次 Compile 的 descriptor 容量不足
- **THEN** 每 draw 的 bindings 在 Compile 冻结并按真实 set 绑定，worker 不写 descriptor；失败恢复本次分配，随后可在同一有效 lease 重试

#### Scenario: 旧新纹理版本同时在途
- **WHEN** 两个提交各自采样旧、新 image 版本，目录被 Prune 且外部 sampler/pins 被释放
- **THEN** 两个提交各自保留原 image/sampler/descriptor 到完成，回读结果保持各自版本，不因新帧绑定而覆写旧帧

#### Scenario: depth 在 scope 与片段之间保留
- **WHEN** D32 attachment 先 CLEAR 再 LOAD，draw 被划分为多个编码片段或 native scope
- **THEN** 仅首个需要清除的片段执行 CLEAR，depth 的 LOAD/STORE、同步及 secondary inheritance 保持有效，串行和并行得到相同遮挡结果

#### Scenario: 稀疏 set 与 layout 切换
- **WHEN** shader 仅使用 set 2，或相邻 draw 切换具有不同 bindings 的 shader layout
- **THEN** native layout 保持真实 set 编号，Pipeline 绑定后重新绑定该 draw 的全部所需 sets 和 dynamic offsets，不沿用不兼容状态

#### Scenario: PSO 复用不混入 draw 绑定数据
- **WHEN** 同一兼容 native scope 内多个 draw 的 program、vertex layout、固定状态及 attachment 签名相同，但 buffer ranges 或 dynamic offsets 不同
- **THEN** 可复用相同 PSO，逐 draw 绑定仍使用各自冻结值；program/layout、vertex stride/attributes、depth/blend 或 attachment 签名不同不得误用同一缓存结果

#### Scenario: 有界跨帧 PSO 与 native scope 缓存
- **WHEN** 后续帧使用相同 immutable pipeline/scope 签名，但 attachment 对象或动态绑定改变
- **THEN** 可复用设备缓存；epoch、shader/layout、vertex/fixed state、附件格式/sample、传统 scope/subpass 不同不得误命中。native scope key 包含 resolve/load/store/layout/dependency，framebuffer/views 仍由当前帧保活；LRU 淘汰只释放 cache owner，失败不入缓存，worker 不查询或修改缓存

#### Scenario: Compute 与 Custom 的声明执行
- **WHEN** 冻结图包含 FrameComputePass 或 FrameCustomPass
- **THEN** 受支持的声明进入后端编译和编码，缺失 encoder、资源访问不合法或超出设备能力时明确失败；不得空分支伪报成功

#### Scenario: shader 生成顶点
- **WHEN** program 的 vertex input 为空，item 使用 shader 生成顶点且 draw args 有效
- **THEN** 可以不绑定 vertex buffer；缺少必需 vertex input 的其它 item 应报告无效并跳过

#### Scenario: 死声明仍验证绑定
- **WHEN** 未被消费的 raster pass 缺少必需 binding、带非法 group override 或与 reflection 不符
- **THEN** culling 前明确失败，与 compute 一致；仅死 pass 的未定义内容读取可不检查

#### Scenario: Metal 同时 resolve 与保存源
- **WHEN** MSAA attachment 声明 resolve，并按 STORE 或 DONT_CARE 选择源保存策略
- **THEN** 原 native render pass 产生合法 resolve 目标；STORE 保留源供后续 LOAD，DONT_CARE 不丢失 resolve 结果

### Requirement: Descriptor resolve 与访问声明

setup SHALL 声明 RG 中图资源的访问和 binding 意图；后端 Compile 解析 backing 后 SHALL resolve descriptor，或为不能提前完成的部分声明显式 CPU 前置任务。消费任务执行前 SHALL 发布不可变 binding 快照。经 descriptor 使用的纹理/缓冲 SHALL 同样声明 RDG Read/Write。最终 bindings SHALL 不被并发编码任务修改，并由 FrameSlot 保活至提交完成。

图参数事务 SHALL 先完成共享 ShaderParameterTransaction 的语义验证，再按 shader/set、typed 字段值和完整资源身份，在同一 composition/lease 内复用 target ABI 编码、uniform upload 和 immutable group。活跃资源变化、范围/view 或布局 owner 变化 SHALL 使相关缓存不兼容；不能仅凭 CPU sizeof、字段名或 dynamic offsets 推断可共享性。native descriptors SHALL 仍由各后端 Compile 按该 lease materialize，worker 不写缓存或参数。

#### Scenario: transient HDR 被合成采样
- **WHEN** ScenePass 写 transient HDR，合成 pass 声明 SRV 读取
- **THEN** descriptor 指向 Compile 解析的同一 backing，图包含写到采样的 barrier，Execute 时无需访问 setup graph

#### Scenario: 编译后 worker 只消费绑定
- **WHEN** Compile 成功后多个 worker 并行编码使用 UBO 的 indexed items
- **THEN** descriptor 内容、PSO 和有效 dynamic offsets 已固定；worker 不解析 shader、不更新 descriptor 或修改共享 cache，所有引用保留至 GPU 退休

#### Scenario: 多 draw 共享语义参数
- **WHEN** 多个 draws 使用相同 Global/Pass/Material 语义值和精确资源，Batch use 的动态窗口不同
- **THEN** 相容 set 的 target 编码/upload 与 native materialization 在其当前图/lease 内复用，Batch offsets 不使其它组重复上传；在途旧快照不被新值覆盖

### Requirement: 后端编译计划身份与单次消费

BackendCompiledFrame SHALL 绑定 backend、device epoch、context identity、slot generation、GraphId 与 PlanId，由其后端管理 native payload。Dispatch SHALL 校验匹配并单次消费计划，保活计划/模块及资源到任务 drain。RecordedFrame SHALL 仅在全部任务退出且组装成功后产出，携带实际编码 receipts；仅匹配的 context 可将其单次提交。编译失败、取消和重复调用 SHALL 不推进全局资源状态。

Dispatch SHALL 在 backend 下转型前同时验证 public identity 与 exact issued plan object；复制 identity 的未知实现或其它 context/backend 的 plan SHALL 显式拒绝。开始编码 SHALL 立即从调用方消费 plan，即使后续 recording 失败也不要求调用方另行 reset 才能安全收齐/丢弃。任何 native command/encoder 失败 SHALL 阻止成功完整 receipt，清理与 CPU drain SHALL 保持闭合。

#### Scenario: 错 context 或过期计划
- **WHEN** plan 被交给其它 context、旧 slot generation 或第二次 Dispatch
- **THEN** 返回可诊断错误，不执行 native 任务或修改当前槽位

#### Scenario: 部分任务失败
- **WHEN** 一部分 native 任务已录制，另一个任务失败
- **THEN** 收齐所有已启动任务、收尾编码对象，不产出可提交 RecordedFrame；GPU 与输出租约按各自未提交协议处理

#### Scenario: 计划尚未释放
- **WHEN** 当前 lease 的编译计划仍存活，调用方尝试重复 Compile 或 Discard
- **THEN** 请求失败且保留原计划；销毁未 dispatch 的计划后才允许无提交回收

#### Scenario: 串行编码结果取消与一次收齐
- **WHEN** 串行 Dispatch 已完成录制但调用方在 TryCollect 前 Cancel
- **THEN** 释放未提交录制并通过正确 ticket 一次返回 Cancelled；已经发生的编码失败仍返回 Failed，错误 ticket 不消费结果，收齐前不能 Discard

#### Scenario: 晚到取消和过期 ticket
- **WHEN** 全部 worker 已完成但尚未收齐时收到 Cancel，或上一 generation 的 ticket 被用于新 session
- **THEN** 前者在 drain 后一次返回 Cancelled 且不组装可提交帧；后者返回错误且不改变新 session。RecordedFrame 已交付后，旧 ticket 不再有取消该录制的权限

#### Scenario: 复制身份的未知 plan
- **WHEN** 未由当前 context 发布的 plan 实现伪造相同 public identity
- **THEN** Dispatch 在下转型和录制前拒绝，合法 issued plan 仍可使用

#### Scenario: 编码启动后失败
- **WHEN** Dispatch 已消费合法 plan，但 native recording 失败
- **THEN** collection 报告失败而不发布成功 receipt；调用方无需保留或手动 reset 已消费 plan，按既有未提交协议安全清理
