# Aurora native backends

- 原生后端遵循根规则及 [Aurora 领域约定](../AGENTS.md) 的 namespace、RHI、同步与 Metal 规则；Aurora 属于 foundation，接口与实现均在 engine/aurora，原生后端保留原 RHI 目录，不迁入 plugins。接口 target 与实现 target 仍分离。
- `common` 只共享 slot 生命周期、lease 约束的 buffer 范围分配、device transient 对象池/借用绑定、完成错误归并、显式纳管资源的提交边界账本与提交保活；native pass、资源/同步计划、encode DAG 和编码策略由各后端完整实现。
- 对外只通过 `DeviceFrameContext` 执行；compiler 为后端私有组合对象。取槽不 reset fence，Rejected 不等待完成，Indeterminate/查询错误不得提前释放资源。
- 每个 command reset 域只能覆盖已经退休的 slot/worker/recording；生产 descriptor/staging/UBO 也必须消费同一 lease，禁止自行推进 ring。
- B11 checked SwapChain 与旧直接协议互斥；Vulkan 需要 maintenance1、acquire fence 与独立 present fence，DX12 使用 DO_NOT_WAIT 和 Present 后 queue fence。资源可退休不等于屏幕已显示；正常 resize/close 不用全设备 WaitIdle，终态销毁另行安全 drain。
- borrowed output 绑定 exact lease/owner，单 mip/layer/X1、UNDEFINED→PRESENT；不得包装成拥有型 ImagePtr。逐 cell 状态表也禁止普通资源声明 PRESENT。depth attachment layout 可能包含固定功能 resolve，Vulkan 同步需覆盖 COLOR_ATTACHMENT_OUTPUT/COLOR_ATTACHMENT_WRITE。
- 当前 Vulkan/DX12 支持 typed buffer copy 冻结图；Vulkan 双 compiler 各自输出 encoder，context mode 固定，支持单 RGBA8 color 的 upload/raster/readback。RenderPass 仅合并连续同 attachment/view、前 STORE/后 LOAD 的 pass 为多 subpass；Dynamic Rendering 独立保留 scope/barrier。公共纯分析不构建 native 同步或命令。
- 编译产物保留 logical/native 映射和模式对应 inheritance，Vulkan parallelNum>1 按实际 worker/slot/ordinal 录制 secondary，coordinator 在 TryCollect 收齐后按计划组装 primary；单 worker 仍 inline。DX12 已具备自己的单 color image/raster compiler/encoder、DIRECT list 并行与 native 故障验证；每片恢复图形状态，仅首片 clear，收齐后一次有序提交。Vulkan/DX12 descriptor/staging/UBO 已按同一 lease 分区并检查容量；B07 已支持多 float stream、U16/U32、D32、set 0/1/2 scalar UBO 与 dynamic offset；扩展 raster/bindings 和 Metal 新协议待实现。colorLocalRead 必须声明且使用 LOAD，两模式当前明确 Unsupported；不得把普通 LOAD/混合当作 shader local read。native 编译对象随 RecordedFrame 保活至 GPU 完成。
- Vulkan/DX12 收齐时 DeviceLost 优先于可恢复错误，错误结果优先于取消；旧 ticket 不得影响新 session。teardown 先请求取消、收齐 CPU，再安全收尾 GPU，最后释放 worker pools；Instance/Device 与 backend module 必须持续存活。

- DX12 紧密 image copy 由 compiler 用 footprint scratch 衔接，RTV/DSV/CBV/scratch 使用 slot 分区与预算，编译失败回滚本次游标；PSO 和计划引用随 compiled plan 保活；不得另起隐式 ring。Close 失败的 list 不能 Reset，须在安全 lease 中重建。原生 queue 操作开始后的失败按 Indeterminate，不能报告 Rejected 后允许重复提交。

- 原生路径沿用 RHI `GetDevice()` 做 exact device 校验，再向对应实现下转型；未知 owner 显式拒绝。不要重复提供 `BelongsTo` 或引入 RTTI。image 描述相等使用 RHI 纯函数，native layout/usage 能力与转换仍由各 compiler 校验。

- B04 分配在 coordinator/Compile 阶段完成，成功 plan 的 borrower 阻止后续写入；UBO size 含对齐 padding。旧宿主 allocator 只供直接协议，B06/B07 不得重用隐式 ring。原生 descriptor reset 失败不发布新 generation；device loss 关闭 context。

- B06 的 RecordedFrame 回执由各后端在所有 live pass/最终转换成功录制并收齐后提供，包含对应冻结图和实际执行 pass；common 仅保存/交付。Compile 成功、计划中的 pass 或 Submit Accepted 均不能冒充 GPU 完成；资源发布归 renderer uploader。

- B07 的 descriptors/PSO 在 Compile 冻结，worker 只发出绑定/绘制命令。Vulkan 稀疏 set 保留空 layout 槽位；失败 Compile 只回收本次新增 descriptor sets，失败的 native 清理不得伪造预算恢复。PSO 当前仅 pass/subpass 内复用，不能误认为 device 跨帧缓存。

- B10 首批支持 2D RGBA8 指定单个完整 mip/layer 的紧密 copy；内容分析逐 mip/layer，initialized=true/整图 export 覆盖全部子资源；initializedRanges 可声明部分完整 mip/layer 有效，与 initialized=true 互斥，UNDEFINED/transient 禁止已有内容。Vulkan/DX12 按物理 backing 的子资源转换，图初末 layout 仍统一，转换不能扩大内容定义范围。第八子批补局部矩形/显式行距；多 layer 单次 copy 仍未支持；第十五子批的 raster 可从多 mip/layer image 选择单 cell；第四子批的 renderer uploader 复用这些 copy 上传完整 image，核验全部回执并成功完成后发布 Resident，后端不负责目录发布。

- B10 第二子批的 transient pool 由 Device 唯一持有，Compile 裁剪后分配完整描述/epoch 匹配的空闲 backing；每个计划共享一个 lease，随编码、录制和提交保活。Rejected/Indeterminate/查询错误不能提前归还，只有未提交安全丢弃、成功完成或安全 teardown 才能复用。固定边界 buffer COMMON→COMMON、image UNDEFINED→GENERAL（DX12 COMMON），不得把缓存旧内容当已初始化。池不构建 barrier/encode 计划，旧 ObjectPool 即时 Release 不得用于新路径。
- B10 第三子批按 live pass 的闭区间分配同图整对象 alias，export 延长到图末；同 pass、区间重叠、描述不同或导入资源不得 alias。各 compiler 按物理 backing 追踪状态，同状态写仍需依赖，final transition 每 backing 一组，按各子资源实际旧状态汇合；RenderPass 合并按逻辑 attachment 身份及其 mip/layer view 判断。当前 GPU 顺序保持 logical pass 顺序；后续重排/跨队列必须重新验证 happens-before，不能照搬这些区间。

- B10 采样绑定支持 RGBA8 单 sample 的 Texture2D/Texture2DArray 显式 mip/layer view 与普通 sampler；只读取暴露的内容，不能在 encoder 暗取 image。array image 的单层切片也要求 array view；同 pass attachment feedback 整对象拒绝。类型反射在 native shader 创建前拒绝未支持声明，Compile 复核 view 形态；未知元数据/descriptor 数组（含长度一）不得按 scalar 接纳。
- image/sampler descriptors 在 Compile 的 lease 分区写入，worker 只绑定。Vulkan sampled view 在计划内按 backing/规范化范围共享，随 encoder/提交退休；失败只清理已成功创建的对象。DX12 SRV 填写实际 mip/layer 范围，随 lease heap 退休。Vulkan 两模式自行安排 SHADER_READ_ONLY 与 VS/FS 同步，DX12 自行建 SRV/sampler heaps 和 root tables；sampler 检查 device 归属后才下转型，图/提交保持引用至安全退休。

- B10 第七子批的内容范围先排序去重，再比较重复导入；不能把两份冲突声明合并扩大初始化承诺。每图声明 image 合计最多 65,536 子资源、每 image 最多 256 个初始化范围，预算在 native 分配前检查。状态表只归存活 canonical backing，worker 只读计划；失败/取消不推进全局状态。
- Vulkan copy/view 仅转换实际范围，同层相邻同旧状态 mips 可合并；RenderPass attachment 显式选择单 mip/layer cell。DX12 image transition 显式填写 subresource，混合状态不得用 ALL_SUBRESOURCES；alias 同状态写仍保留依赖。帧末转换全部使用 backing 的子资源到各自声明的 final，部分内容与统一最终 layout 分开管理。

- B10 第八子批沿用 BufferImageCopy：rowLength/imageHeight 单位为 texel，0 为紧密默认；局部矩形只定义实际 texels，buffer padding 不读写，末行不要求尾部 padding。当前 RGBA8 单层 copy 的 rowPitch 字节上限 INT32_MAX，全图读写内容区间合计最多 65,536，地址/范围/展开预算在 native 分配前检查。跨图 initializedRanges 表示完整 mip/layer，initializedRegions 表示 RGBA8 矩形；两者取并集，局部上传不自动发布 Resident。
- Vulkan 传递完整 region，barrier 仍按 subresource；DX12 按矩形查询 footprint、保留 lease scratch 重排，未初始化 RT 首次局部写先 discard/transition 初始化 metadata。imageReady 归本次 Compile 的 canonical backing，已有有效内容或后续 patch 不得 discard；metadata ready 不代表整个子资源内容有效。

- B10 第九子批统一用 ResolveInitializedImageContents 校验完整范围/矩形并归一化为 texel spans；每 image 合计最多 256 条初始化声明，全图初始区间合并前最多 65,536，死资源也计费。矩形入口只支持 RGBA8 2D 单 sample，UNDEFINED/transient 禁止已有内容；完整采样/LOAD/export 仍检查全部所需 texels。
- 跨图声明必须来自 owner 已确认安全可用的内容；本子集以前图成功完成后接续为验收边界，不生成全局提交账本。DX12 含任何已知有效 texel 的子资源不得再次 discard；原生 metadata 可用性不扩大逻辑内容。

- B10 第十子批 AvailableFrameBufferBytes 只查询可分配 lease 的对齐后容量，不创建 buffer 或推进游标。分块 uploader 只导入成功完成的前缀，GENERAL 统一边界不能冒充通用状态账本；原有 native recording/dispatcher/scratch 预算继续独立校验。

- B10 第十一子批 FrameGraphBuffer.initializedRanges 只声明字节有效性，最多 256 条，经 ResolveInitializedBufferContents 规范化；全初始化与范围互斥、transient 不携带旧内容。重复导入比较覆盖集合，copy/raster/export 不得跨缺口；byte 内容不改变既有 native buffer 同步粒度或边界状态。

- B10 第十二子批 TrackResource/GetResourceState/UntrackResource 只管理同一 context/graphics queue 的边界（第十四子批支持逐 mip/layer）。资源必须由该 context 独占状态修改；纳管后不允许 legacy、其它 context/queue 或外部直接改写。强 pin 固定精确 backing 身份，默认最多 1024 条，手动 owner 在全部使用回执退休后显式移出；第十三子批的拥有租约条目由安全入口自动退休。
- Compile 候选只来自裁剪后使用的非 transient bindings；initial 必须匹配 accepted 状态，native compiler 仍各自生成同步。Accepted 才发布 final/token，completed 按 serial 单调推进；旧 slot 未退休或计划借用存活时不允许移出/重登记。取消/失败/Rejected 不误发布，Indeterminate/完成错误关闭账本并保活至安全 teardown。
- 状态账本不存储有效内容，不扩大 initialized 声明、不发布 Resident、不新增 GPU wait。ResourceUploader 自动纳管/退休已在第十三子批接通；单 context 已纳管资源不可走手动 AllocateCommand/FinishRecording，Metal 新入口明确 Unsupported。

- TrackOwnedResource 的返回租约不保存 context 指针，账本只弱引用 owner；最后 owner 释放不调用 GPU/context。Track/TryBeginSlot/DiscardUnsubmitted/PollCompleted 在协调线程确认 owner 过期、pending=0 且无计划借用后回收；终态只走安全 teardown。不可 Untrack 活跃的受管租约，不允许为腾容量驱逐 Resident/回退/冻结快照仍拥有的条目。
- uploader backing 的租约随 Resident 版本转交；Compile 前须保留版本/租约，裸 native 指针图不自动持有此 owner。frame accepted 状态才是后续导入依据，backing.state 仅记录上传发布边界；CPU 内容有效性与账本状态仍分离。

- 第十四子批 ImageBoundaryState 共用于图与账本，统一 layout 或完整 layer-major/mip-major 数组；边界和每 cell 内容先校验，原生同步仍由各 backend 构建。Vulkan 最终按相同旧/新状态合并连续 mips，DX12 使用显式 Subresource；无 pass export/未触及 cell 也必须兑现 final。
- 通过已验证 owner 的 Image::GetMipLayerCount 获取实际 cells；未知 storage=0 拒绝。trackedImageSubresources 默认 65,536，独立于 trackedResources，manual/owned 安全退休均归还；不因声明为统一 layout 免除真实 cells 的计费。transient 仍保持 UNDEFINED→GENERAL 的统一边界语义。

- 第十五子批 RasterAttachmentView{mip,layer} 用纯函数解析 range/index/extent，RGBA8 color 与 D32 depth 的选中 extent 必须相等。VulkanGraphImageView 复用计划内 view 缓存；两 compiler 分别持有依赖，framebuffer 先于 view 销毁。DX12 显式 RTV/DSV 选择 mip/array slice，viewport、状态与 metadata-ready 标记对应所选 cell；descriptor 继续归现有 lease 分区。第十六子批已接 RGBA8 color MSAA/resolve，第十七子批已接同采样数 D32 depth；layered/depth resolve 后续验收。

- RGBA8 color 与 D32 depth 的 X2/X4/X8 MSAA 分别以 native format/usage/sample 查询为准，查询在 live transient 分配前，不能假定全部设备支持；pool 逻辑字节包含 samples。Vulkan 两套 compiler 各自生成 attachment resolve 与匹配 samples 的 PSO/inheritance，带 resolve 的 RenderPass 暂不合并；DX12 resolve task 在最后 raster fragment 后执行，仍在自己的 context 下 dispatch。
- DX12 initializeImage/imageWriteBarrier 由局部 copy 与 resolve 共用，只初始化未就绪 RT cell 的原生 metadata；先在 RT 状态 Discard 再进入实际写状态，已有内容不被丢弃，逻辑定义范围仍来自 graph。resolve views/RTV/任务 backing 沿既有 lease/submit 退休，workers 不查询能力或创建 view。
- D32 MSAA 的 extent/sample count 必须匹配 color，LOAD/STORE 仅作用于所选 cell 的全部 samples；color resolve 不定义 depth。RenderPass 三附件下 resolve index 追加到实际 attachmentCount，depth 使用实际 samples；DX12 DSV 区分 TEXTURE2DMS/ARRAY。能力检查、views/DSV/PSO 构建仍在 Compile，取消/drain/GPU 完成前不得复用 depth backing。


- B10 收尾已接 D32 MINIMUM/MAXIMUM depth resolve：源 CLEAR/合法 LOAD、目标 X1 D32/所选 extent 相同；源丢弃不丢弃目标。能力在 transient 分配前检查；Vulkan 两套 scope 独立，DX12 使用 CommandList1，先初始化未就绪目标 depth metadata，再执行 resolve。
- device PSO/RenderPass cache 按 epoch 与不可变签名隔离，各自最多 128 项；cache owner 淘汰不越过帧保活，device 原生销毁前清 cache。workers 不查能力、不修改 cache/descriptor；framebuffer/views 仍归帧。
- 编码收齐后 GPU bindings/encoder/shader/sampler/payload 转交 slot，CPU 图仅由待提交 receipt 保留。Rejected 保留 receipt；Accepted/Indeterminate 后清理 receipt，GPU refs 继续按完成/隔离退休，不能用保留整图代替明确 GPU pin。
