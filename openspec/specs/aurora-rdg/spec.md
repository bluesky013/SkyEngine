# aurora-rdg Specification

## Purpose
Specify Aurora frame graph declarations, immutable ownership, resource content and lifetime analysis, backend-specific compilation and encoding, and checked submission boundaries for supported renderer backends.

## Requirements
### Requirement: RenderGraph 三段式 lifecycle

RDG SHALL 保留 Setup -> Compile -> Execute 的语义阶段，具体入口为公共声明图 Freeze、对应后端完整 Compile、对应 DeviceFrameContext Dispatch/收齐编码结果；Queue Submit SHALL 为显式后续步骤。公共层 SHALL 提供 IR、诊断和可复用纯分析，不固定所有后端的 native pass、编码任务或 command buffer 数量。Compile SHALL 不执行 acquire、present、队列提交或编码 jobs。

#### Scenario: 由活动后端编译
- **WHEN** 同一份合法声明图交给已支持新协议的后端 FrameContext
- **THEN** 由相应后端生成匹配的 compiled plan，满足相同逻辑语义但允许不同 native scopes、编码任务和命令组合；尚未支持的后端明确返回 Unsupported

#### Scenario: dispatch 只读编译产物
- **WHEN** 编译结果进入 Dispatch
- **THEN** 编码任务只读其已冻结 payload/bindings 与显式前置任务发布的快照，不读取可变 setup graph、Scene ECS 或宿主对象

#### Scenario: Freeze 封口与保活
- **WHEN** builder 已 Freeze，调用方随后销毁 builder 和外部输入引用
- **THEN** builder 拒绝追加或修改声明，冻结图拥有 IR 和所需输入；编译/录制/提交依次保活资源，GPU 完成前不得覆写其仍在使用的内容

#### Scenario: 帧末状态转换
- **WHEN** 输出声明了最终状态
- **THEN** 后端在消费者完成后的合法 native 边界发出对应同步，候选最终状态仅在提交接受后进入账本

### Requirement: Resource declaration: Import vs Create

RenderGraph SHALL 区分引用计数外部资源、borrowed swapchain output 与 transient 资源。普通 ImagePtr/BufferPtr 导入 SHALL 保留引用；swapchain image SHALL 由 swapchain 唯一持有，通过 borrowed lease 导入，不得构造会删除该 image 的 CounterPtr。CreateTexture/CreateBuffer SHALL 使用 transient pool。资源名称 SHALL 使用 core::Name。提交使用的资源引用/leases SHALL 转交 FrameSlot 保活至完成，不能仅依赖 CPU graph 的生命周期。

#### Scenario: Import 外部引用计数资源
- **WHEN** 调用方导入普通 ImagePtr 后释放自身引用并结束 CPU graph 生命周期
- **THEN** 若 GPU 提交仍使用该资源，FrameSlot 的引用保持其存活至提交完成

#### Scenario: 借用原生输出
- **WHEN** scheduler 将已获取的 swapchain 图像导入当前图
- **THEN** 图像引用带 exact DeviceFrameLease 与独立 owner pin，当前输出子集为单 sample/mip/layer 的 RGBA8/BGRA8 UNORM/SRGB、UNDEFINED→PRESENT，不承诺旧内容。重复导入必须具有相同借用寿命，普通资源格式能力不因输出支持而扩大

#### Scenario: 非输出资源伪造 PRESENT 边界
- **WHEN** 普通拥有型 image 的统一或任一 mip/layer 初末状态声明 PRESENT
- **THEN** Compile 显式拒绝，即使该 cell 没有被 pass 访问；不分配 native 资源，不遗留阻止合法同 lease 重试的借用

#### Scenario: 同一物理 image 重复导入
- **WHEN** 两个 feature 将同一 ImagePtr 以相同存储描述、初末状态和初始化声明导入同一 builder
- **THEN** 返回同一逻辑 handle，不重复占用资源预算；调试名不参与身份，冲突声明显式失败且不修改先前声明，冻结 IR 不允许绕过 builder 构造重复物理身份

#### Scenario: 部分初始化导入与等价范围
- **WHEN** 持久 image 声明部分完整 mip/layer 或受支持的 RGBA8 矩形已初始化，重复导入使用重叠、重排或拆分的等价范围
- **THEN** 内容依赖从该集合开始；等价集合与相同初末状态返回原 handle，冲突不扩大已有声明，未声明内容仍未定义。完整子资源范围与矩形取并集，全量 initialized 与这两类部分声明互斥，transient/UNDEFINED 初态禁止已有内容，非法范围与超预算在 native 分配前拒绝

#### Scenario: 部分内容与全图导出
- **WHEN** 只初始化部分子资源的 image 被 view/copy 读取或整图 export
- **THEN** 范围内读取可成功，全图 export 仍要求所有子资源有效；最终 layout 转换不自动定义未写内容，部分导入不使 uploader 发布部分 Resident

#### Scenario: 已完成矩形跨图补齐
- **WHEN** 前图成功完成的矩形以相同 backing 和实际统一边界导入下一图，下一图追加局部 copy
- **THEN** 内容验证从已声明 texel 集合开始，补齐后的读取/完整 view/LOAD/export 才能成功；缺口及其它 mip/layer 不被定义，导入内容不能因后端 metadata 初始化而被 discard

#### Scenario: 矩形导入预算与声明校验
- **WHEN** 完整范围与矩形初始化声明总数、行区间展开或 texel 地址超出预算/范围，包括没有存活 pass 的资源
- **THEN** 在 native 分配前显式拒绝；每 image 最多 256 条初始化声明，全图合并前初始内容区间最多 65,536，完整宽度按连续区间计费；手工 FrozenFrameGraph 不能绕过校验

#### Scenario: 内容导入不冒充上传完成
- **WHEN** 某图已 Compile、录制或提交 Accepted，但 owner 尚不能证明待续接内容可安全使用
- **THEN** 不自动扩大后续图的初始化声明，不发布部分 Resident；调用方负责内容与实际边界的真实性，部分导入本身不构成跨图提交账本

#### Scenario: Import swapchain output
- **WHEN** 调度器导入已获取的 swapchain output lease
- **THEN** graph 能引用其 Image，且 graph 销毁不释放 swapchain 所拥有的 image

#### Scenario: CreateTexture 命中可用池资源
- **WHEN** 相同描述的池资源没有未完成提交占用
- **THEN** 新 transient 分配可复用它，调试统计显示 hit

#### Scenario: 显式完整 image 边界表
- **WHEN** image 使用统一 layout 或逐 layer/mip 的完整 initial/final 表
- **THEN** 显式表长度严格匹配 mipLevels*arrayLayers，空表/缺项/多项/非法枚举拒绝，全部 final 均非 UNDEFINED；等值数组与统一 layout 等价，重复导入不能合并冲突。Import 与手工 FrozenFrameGraph 的 Analyze 都校验，死声明仍计入图子资源预算

#### Scenario: UNDEFINED 只禁止对应子资源已有内容
- **WHEN** 部分 cells 的 initial 为 UNDEFINED，另一些 cells 携带 initializedRanges 或 initializedRegions
- **THEN** 只有有效 texel 落在 UNDEFINED cell 时拒绝；其它 cell 的内容不受牵连，布局有定义也不补齐内容缺口，全 image export 仍要求全部内容

### Requirement: Pass 类型与 setup builder

`RenderGraph` SHALL 提供三种 pass 类型，每种对应一个 builder：

- `AddRasterPass(name, setup_fn, exec_fn)` — `RasterPassBuilder` 含 `ColorAttachment(slot, handle, loadOp, storeOp)`、`DepthStencilAttachment(handle, depthLoad, depthStore, stencilLoad, stencilStore)`、`Read(handle, AccessFlags)`、`SetViewport/Scissor`
- `AddComputePass(name, setup_fn, exec_fn)` — `ComputePassBuilder` 含 `Read(handle, AccessFlags)`、`Write(handle, AccessFlags)`
- `AddCopyPass(name, setup_fn, exec_fn)` — `CopyPassBuilder` 含 `Src(handle)`、`Dst(handle)`

setup callback 在 `AddXxxPass` 调用内立即执行；exec callback 在 `Execute()` 时被调用，签名为 `void(EncoderType &, RDGContext &)`。

#### Scenario: Raster pass 含 attachment + read texture
- **WHEN** Raster pass setup 声明 `ColorAttachment(0, gbuffer)` + `Read(shadowMap, FRAGMENT_SRV)`
- **THEN** Compile 期 RDG 知道：gbuffer 必须在此 pass 之前 transition 到 COLOR_ATTACHMENT；shadowMap 必须 transition 到 SHADER_READ_ONLY；本 pass 在 shadowMap-writer pass 之后执行

#### Scenario: Compute pass 写 storage buffer 后由 graphics pass 读
- **WHEN** ComputePass A 声明 `Write(buf, COMPUTE_UAV_WRITE)`；RasterPass B 声明 `Read(buf, FRAGMENT_SRV)`
- **THEN** Compile 推导：A 必须在 B 之前；A 与 B 之间 emit memoryBarrier(srcAccess=COMPUTE_UAV_WRITE, dstAccess=FRAGMENT_SRV, srcStage=COMPUTE_SHADER, dstStage=FRAGMENT_SHADER)

### Requirement: Compile 推导 barrier 并自动 emit

各后端 Compile SHALL 按物理资源的 GPU 使用顺序和 native pass scope 推导合法同步，由 FrameContext Dispatch 的编码计划自动 emit；同步可为 barrier 或满足该后端要求的 encoder 边界/其它原语。pass 回调 SHALL NOT 自行插入未声明的资源 barrier。导入资源 SHALL 声明实际初始状态及所需最终状态；新建且允许丢弃内容的资源可从 UNDEFINED 开始，跨帧持久图像不得无条件假定 UNDEFINED。重复导入同一物理资源 SHALL 统一身份与状态；同帧物理复用 SHALL 包含跨逻辑 handle 的同步。export 的最终状态 SHALL 按声明处理，窗口输出为 PRESENT。CPU 任务依赖 SHALL 不被当作 GPU 内存同步。

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

### Requirement: Pass culling 自动剔除未消费 pass

Compile SHALL 从有效导出输出、显式 MarkOfInterest、获准持久上传和其它明确副作用根反向保留必要内容依赖；普通 Import 本身 SHALL 不自动成为活跃根，失效 viewport 输出不得成为活跃根。未消费的 transient 写入 SHALL 被剔除且不分配 backing。依赖不可用资源的 pass SHALL 不执行并报告原因；仍有有效 consumer 的共享生产者 SHALL 保留。调试信息 SHALL 列出 live/culled pass 与原因。

#### Scenario: 写未读的 transient pass 被剔除
- **WHEN** Pass A 写 transient T，T 无后续消费者且未标记为输出
- **THEN** A 不执行，T 不分配实际 image

#### Scenario: 一个窗口输出失效
- **WHEN** 某窗口输出不可用，但同一 scene 的另一输出仍可用
- **THEN** 仅保留有效输出所需的依赖，失效分支不产生 Present 或访问空 backing

#### Scenario: 没有活跃根的图
- **WHEN** 图只有普通 imports，其所有 pass 都没有导出消费者或明确副作用
- **THEN** 编译返回无工作结果，不创建一次伪提交，调用方能直接释放未提交 lease

### Requirement: Transient pool 复用底层 RHI 资源

RDG SHALL 使用 device 级 transient pool，按完整兼容描述（含尺寸、格式、samples、usage）与使用区间选择 backing。不同未完成提交 SHALL 通过 frame leases 或 slot 分区隔离。单图内不重叠逻辑资源可复用同一 backing，但 SHALL 保留其物理访问同步。淘汰 SHALL 只释放无在途引用的资源，不得仅按 CPU 帧数判断安全。

#### Scenario: 同帧内不重叠 transient 复用
- **WHEN** 多个兼容 texture 的使用区间不重叠且允许丢弃旧内容
- **THEN** 可以复用同一 Image，并包含前后物理使用间必要 barrier

#### Scenario: alias 依据存活使用顺序而非声明顺序
- **WHEN** 图内 transient 的声明顺序与存活 pass 使用顺序不同，或存在被剔除的写入
- **THEN** 按实际 GPU 顺序的首末存活访问计算闭区间，读取、写入与 DONT_CARE 副作用均计入；完整描述相同且严格不重叠者可共用一个原生对象，死资源不分配

#### Scenario: 导出和同 pass 使用阻止过早 alias
- **WHEN** 资源被导出到图末、在同一 pass 被共同使用，或两个资源的存活区间重叠
- **THEN** 不把它们绑定到同一 backing；导入资源与不兼容描述也不参与 transient alias，GPU 顺序改变时重新验证可复用区间

#### Scenario: alias 不继承旧逻辑资源内容
- **WHEN** 新逻辑资源复用已有 backing，但只定义部分 buffer 范围或部分 image mip/layer
- **THEN** 读取其它未定义内容仍失败，即使旧逻辑资源曾写入那些 bytes/texels；状态转换不扩大内容定义范围

#### Scenario: alias 保留物理状态与同状态写依赖
- **WHEN** 一个 backing 从前一逻辑资源交给后一逻辑资源，包括丢弃内容后以相同访问状态写入
- **THEN** 后端沿实际物理状态生成必要同步，不因逻辑内容独立或状态相同而省略写依赖；每个 backing 只执行一组最终边界转换，RenderPass 不因 backing 相同而合并不同逻辑 attachment

#### Scenario: alias 组共享借用和物理容量
- **WHEN** 多个逻辑资源在本图内映射到同一个 transient backing
- **THEN** 池容量按实际 backing 计数，整个组共享本计划的退休约束；其它未完成计划不得借用该对象，编译失败、取消与不确定提交沿用相同释放或隔离协议

#### Scenario: 跨帧稳定复用
- **WHEN** inflight ring 预热后持续使用相同描述且对应提交已完成
- **THEN** 后续帧复用空闲 backing；GPU 未完成的 backing 不被提前分配给另一提交

#### Scenario: 裁剪后才分配 transient backing
- **WHEN** 冻结图含未被根或存活 consumer 读取的 transient 声明/写入
- **THEN** Compile 不为其分配 backing；存活图的物理映射只存在于后端计划中，不改写冻结图及录制回执身份

#### Scenario: transient 借用跨编译编码提交保留
- **WHEN** 存活 transient 已被编译计划借用，随后进入编码、RecordedFrame 或提交
- **THEN** 其它计划不得重用其 backing；编译失败/未提交丢弃释放本次借用，Rejected 保留 recording 的借用，成功完成才归还已提交借用

#### Scenario: 不确定提交与查询错误不提前归还
- **WHEN** transient 所在提交返回 Indeterminate 或其完成查询失败
- **THEN** 错误通知不归还借用，资源继续隔离直到能够确认安全完成或安全 teardown

#### Scenario: 有界池的淘汰与 epoch 隔离
- **WHEN** 新图需要不同描述或不同 device epoch 的 backing，或命中池容量限制
- **THEN** 只复用同 device/epoch 的兼容空闲对象；淘汰只涉及无借用对象，不足时显式失败并清理本次借用，不驱逐在途对象

#### Scenario: MSAA transient 容量与复用
- **WHEN** 图分配多采样 transient image
- **THEN** 逻辑字节预算包含 samples 倍数且防溢出，sample count 不同不得作为同描述复用；未退休借用保持占用，容量拒绝不创建 backing，只有安全退休后才能复用

### Requirement: Resource 与 pass 名称使用 core::Name

RDG 的资源名与 pass 名 SHALL 使用 `core::Name`（`sky::Name`）而非 `std::string`。所有 `name` 参数与内部存储（`ResourceNode::name`、`PassNode::name`）均为 `Name` 类型。

`Name` 提供 interning 语义：相同字符串对应相同 handle，`operator==` 为 O(1) 比较，`std::hash<Name>` 可用作 unordered 容器 key。

#### Scenario: 相同名称得到相同 Name 身份
- **WHEN** 两个 pass 用相同字符串字面量构造 `Name("post-process")`
- **THEN** 两个 `Name` 相等（`==` 成立），可作为一致的身份标识

### Requirement: CompiledPass variant payload

公共 logical payload SHALL 保持 SceneRaster、FullScreen、Compute、CopyBlit、Present、Custom 的类型语义，供后端降为自己的 native pass/encode plans，不要求 native 产物沿用统一 variant 或 flat pass 列表。编译产物 SHALL 保存每个 logical pass 解析后的有效 set 0 binding；整图 globalResourceGroup 仅可作为显式 fallback。SceneRaster payload SHALL 保存当帧收集的 DrawItems，编码任务 SHALL 只读已发布的 compiled 数据。

#### Scenario: SceneRaster payload
- **WHEN** 带有 DrawItems 和 View binding 的 SceneRaster pass 被编译
- **THEN** payload 保留这些 items 及该 pass 的有效绑定，不从共享可变 View 成员重新读取

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

### Requirement: 基础 pass builder 与 API

`RenderGraph` SHALL 提供 6 种基础 pass 的 builder 与 `AddXxxPass` API：

- `AddSceneRasterPass(name, setup, exec)` → `SceneRasterPassBuilder`（`ColorAttachment` / `DepthStencilAttachment` / `AddDrawItem`）
- `AddFullScreenPass(name, setup)` → `FullScreenPassBuilder`（`SetTechnique` / `SetPassResourceGroup` / `SetTarget` / `SetDepthStencil` / `SetInputSRV`）
- `AddComputePass(name, setup)` → `ComputePassBuilder`（`SetPipeline` / `SetPassResourceGroup` / `SetGroups` / `Read` / `Write`）
- `AddCopyBlitPass(name, setup)` → `CopyBlitPassBuilder`（`Src` / `Dst` / `SetKind` / `SetSize` / `SetOffsets`）
- `AddPresentPass(name, setup)` → `PresentPassBuilder`（`SetSource`）
- `AddCustomPass(name, setup, exec)` → `CustomPassBuilder`（`SetCallback`）

`Compile.cpp::ProduceCompiledGraph` SHALL 从 `SceneRasterPassData.items` 拷贝到 `SceneRasterPayload.items`。

`Execute.cpp::ExecutePasses` SHALL `switch (pass.type)` dispatch 到各 payload 的 emit 逻辑（固定 set index 绑定：`set 1 = Pass ResourceGroup`，`set 2 = Batch ResourceGroup`）。

#### Scenario: SceneRasterPass 收集 DrawItem
- **WHEN** `SceneRasterPassBuilder.AddDrawItem(item)` 多次调用
- **THEN** `SceneRasterPassData.items` 按调用顺序收集；`Compile()` 后 `SceneRasterPayload.items` 包含相同顺序的 item

#### Scenario: FullScreenPass 构建
- **WHEN** `graph->AddFullScreenPass("bloom", [&](FullScreenPassBuilder &b) { b.SetTechnique(pso); b.SetPassResourceGroup(rg); b.SetTarget(tex); })`
- **THEN** 产出 `CompiledPass{type=FULLSCREEN, payload=FullScreenPayload{pso, rg, colors}}`；executor `BindResourceGroup(1, rg)` + `Draw(3,0,1,0)`

#### Scenario: CustomPass 逃生门
- **WHEN** `graph->AddCustomPass("dlss", setup, [](RDGContext &ctx, CommandBuffer &cmd) { /* device 扩展 */ })`
- **THEN** `CompiledPass{type=CUSTOM, payload=CustomPayload{fn}}`；executor 调用 `fn(ctx, cmd)`

### Requirement: SceneRaster pass execute 由 DrawItem 驱动

AddSceneRasterPass SHALL 只接收 setup，execute SHALL 由 DrawItems 驱动。DrawItem SHALL 携带 PSO、几何 streams、index type/offset、draw args、batch RG 和 dynamic offset。executor SHALL 先绑定 Pipeline，再绑定兼容的 set 0/1/2；切换 layout 后 SHALL 重新绑定所需 sets。batch RG 非空时 SHALL 应用 batchDynamicOffset。几何绑定 SHALL 保留实际 vertex streams、offsets 和 U16/U32 index type，不得硬编码单 stream 或 U32。

#### Scenario: SceneRaster items 驱动绘制
- **WHEN** 收集的 item 具有有效 PSO 与 batch RG
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

#### Scenario: 明确未实现的 Compute 与 Custom
- **WHEN** 新 IR 包含 FrameComputePass 或 FrameCustomPass，即使没有 exports
- **THEN** Compile 明确返回 Unsupported，不执行任意回调，也不将其作为空图成功；Fullscreen 的 generated triangle 通过既有 raster encoder 绘制和读回

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

### Requirement: RDGTextureHandle / RDGBufferHandle 强类型 opaque ID

Aurora RDG SHALL 提供两个独立的 handle 类型：

```cpp
struct RDGTextureHandle { uint32_t id = INVALID_INDEX; };
struct RDGBufferHandle  { uint32_t id = INVALID_INDEX; };
```

不允许隐式转换。Handle 仅在所属 RenderGraph 实例的生命周期内有效；跨 graph / 跨帧使用 handle 是未定义行为。

`IsValid()` 方法返回 `id != INVALID_INDEX`。

#### Scenario: 类型不匹配编译期拒绝
- **WHEN** 调用方 `b.Read(textureHandle, ...)` 但 builder 的 Read 签名期待 `RDGBufferHandle`
- **THEN** 编译错误（强类型保护）

#### Scenario: 跨 graph handle 使用被检测
- **WHEN** Debug build 下，从 graph A 拿到的 handle 被传给 graph B 的 Read
- **THEN** assert 触发（handle 内部带 owner-graph epoch tag 用于校验）

### Requirement: Handle 生命周期分析

Compile 期 SHALL 对每个 handle 计算 `(firstUsePass, lastUsePass)` 区间。该区间用于：
- Transient pool alias 决策
- 推导 export 资源（lastUsePass 是终点的 import-原点资源）的最终 transition
- Pass culling（lastUsePass 决定何时"释放"transient slot）

Read pass / Write pass / Attachment pass 都计入使用。

#### Scenario: 单一 use pass
- **WHEN** Texture T 仅被 Pass A 写
- **THEN** firstUsePass = lastUsePass = A.index；transient pool 在 A 结束后立即可回收 T

#### Scenario: 跨多 pass 使用
- **WHEN** Texture T 被 Pass A 写、Pass C 读、Pass D 读
- **THEN** firstUsePass = A.index；lastUsePass = D.index；T 在 [A, D] 区间内被池占用

### Requirement: RDGContext 提供运行时资源访问

execute lambda 收到 `RDGContext &ctx` 参数。`ctx` SHALL 提供：

- `GetTexture(RDGTextureHandle) -> Image*` — 返回当前 pass 中该 handle 实际绑定的 RHI Image
- `GetBuffer(RDGBufferHandle) -> Buffer*` — 同上
- `GetPassName() -> std::string_view` — 调试用，由 `Name::GetStr()` 提供
- `GetCommandBuffer() -> CommandBuffer*` — 与 Execute 传入的同一对象

execute lambda **不得**调用 `GetCommandBuffer()->PipelineBarrier`（barrier 由 RDG 提供）；MAY 调用 cmdBuf 上的非 barrier 接口（如 push constants 等需要 cmdbuf-级访问的工具，理论上）。

#### Scenario: execute lambda 取得当前 RHI 资源
- **WHEN** Compute pass 内 `auto *buf = ctx.GetBuffer(handle); auto *encoder = ...; encoder->BindResourceGroup(0, group);`
- **THEN** buf 是本帧 transient 池实际分配/复用的 RHI Buffer*；group 中的 buffer 写入对应该 buf

#### Scenario: execute lambda 取得 pass 名称
- **WHEN** Raster pass 内 `std::string_view n = ctx.GetPassName();`
- **THEN** n 为本 pass 注册时传入的 `Name` 对应的字符串内容（interning 存储稳定，view 生命周期安全）

### Requirement: Handle equality 与 hash

RDG handle SHALL 支持 `operator==` / `operator!=`；MAY 提供 `std::hash<RDGTextureHandle>` 特化以便用作 unordered_map key（用于 pass 内的 ResourceGroup 缓存等场景）。

#### Scenario: handle 作为 map key
- **WHEN** 调用方 `std::unordered_map<RDGTextureHandle, CounterPtr<ResourceGroup>> cache;`
- **THEN** 编译通过；cache 操作正常

### Requirement: CopyBlit pass 可声明显式访问类型

`CopyBlitPassBuilder` SHALL 提供 `Src(RDGTextureHandle, AccessFlags)` 与 `Dst(RDGTextureHandle, AccessFlags)`（`Dst(RDGBufferHandle)` 保持 COPY_DST）。内置 fullscreen blit 渲染路径 SHALL 以 `SRV` 声明 src、以 `RTV` 声明 dst，使 barrier 推导把 src 置 `SHADER_READ_ONLY`、dst 置 `COLOR_ATTACHMENT`；拷贝路径 SHALL 继续用默认 `COPY_SRC` / `COPY_DST`。

#### Scenario: 采样 blit 的 barrier

- **WHEN** 一个 CopyBlit pass 以 `Dst(dst, AccessFlagBit::RTV)` / `Src(src, AccessFlagBit::SRV)` 声明
- **THEN** 编译后该 pass 的 front barriers 把 src 转 `SHADER_READ_ONLY`、dst 转 `COLOR_ATTACHMENT`

#### Scenario: 拷贝 blit 保持默认

- **WHEN** 使用 `Dst(dst)` / `Src(src)` 两参重载
- **THEN** 访问类型仍为 `COPY_DST` / `COPY_SRC`

### Requirement: Descriptor resolve 与访问声明

setup SHALL 声明 RG 中图资源的访问和 binding 意图；后端 Compile 解析 backing 后 SHALL resolve descriptor，或为不能提前完成的部分声明显式 CPU 前置任务。消费任务执行前 SHALL 发布不可变 binding 快照。经 descriptor 使用的纹理/缓冲 SHALL 同样声明 RDG Read/Write。最终 bindings SHALL 不被并发编码任务修改，并由 FrameSlot 保活至提交完成。

#### Scenario: transient HDR 被合成采样
- **WHEN** ScenePass 写 transient HDR，合成 pass 声明 SRV 读取
- **THEN** descriptor 指向 Compile 解析的同一 backing，图包含写到采样的 barrier，Execute 时无需访问 setup graph

#### Scenario: 编译后 worker 只消费绑定
- **WHEN** Compile 成功后多个 worker 并行编码使用 UBO 的 indexed items
- **THEN** descriptor 内容、PSO 和有效 dynamic offsets 已固定；worker 不解析 shader、不更新 descriptor 或修改共享 cache，所有引用保留至 GPU 退休

### Requirement: 读取版本与内容依赖

RDG SHALL 将每次 Read 绑定到其逻辑顺序前的生产版本，区分保留旧内容的 ReadWrite 和有完整覆盖证明的 WriteDiscard。attachment LOAD/部分写及其它保留内容操作 SHALL 追溯必要旧 producer；STORE_DONT_CARE 和未初始化范围不得被后续读取当作有效内容。Culling SHALL 沿内容依赖进行，之后按存活访问重建 RAW/WAR/WAW 和显式执行顺序，不能用资源的最终 lastWriter 代替读取来源。

#### Scenario: 中间读取与后续覆盖
- **WHEN** A 写 R，B 读 R 并产生导出 O，C 完整覆写 R 且无 consumer，只有 O 是根
- **THEN** A/B 保留，C 可剔除；B 读取 A 版本；若 R 的最终版本也被导出，则 C 保留且 B 在 C 覆写前读取

#### Scenario: UI LOAD 保留底图
- **WHEN** Scene 写 color，UI pass 使用 LOAD 在同一 color 上叠加并导出
- **THEN** UI 的内容依赖保留 Scene，不能只保留最后一个 UI writer

#### Scenario: 多段覆盖先前未定义读取
- **WHEN** A 从未定义范围复制到 R，B/C 分别完整覆盖 R 的两半，D 读取完整 R 并作为明确副作用根，且没有其它消费者依赖 A
- **THEN** A 被剔除，B/C/D 保留并定义 D 所需内容；不能仅因旧写的顺序冲突保留 A

#### Scenario: 单 mip/layer 写入不定义整图
- **WHEN** 未初始化 image 仅有一个完整 mip/layer 被写入，之后读取其它 mip/layer 或导出整个 image
- **THEN** 编译因未定义内容失败；只有已写入的子资源可读取，initialized 导入声明表示全部声明的 mip/layer 有效

#### Scenario: 子资源独立剔除与读取版本
- **WHEN** 不同 mip/layer 被分别写入，同一子资源在两次导出读取之间再次覆盖
- **THEN** 各读取保留自身所需的前序 writer；其它子资源的写入不覆盖其版本，被完整覆盖且无消费者的旧写可以剔除

#### Scenario: buffer 部分字节导入与物理身份
- **WHEN** imported buffer 声明 initializedRanges
- **THEN** offset/size 必须非空、无溢出且位于完整 buffer size 内，最多 256 条；范围排序合并后用于重复物理导入比较，顺序/重叠/拆分等价，冲突拒绝；initialized=true 与范围互斥，transient 不能声明已有内容

#### Scenario: 任意 buffer 读取不跨有效内容缺口
- **WHEN** copy、image upload、vertex/index/UBO 或 export 读取部分初始化的 buffer
- **THEN** 所需字节必须被初始范围和此前存活的写入完整覆盖，单字节缺口也拒绝；写入补齐后可读，声明不产生 roots、不保留已死的覆盖写、不继承 alias 旧内容

#### Scenario: 已完成前缀与候选 copy 共同定义完整版本
- **WHEN** 分块 uploader 导入已经成功完成的 RGBA8 前缀并声明当前块写入
- **THEN** 完整层/mip 与局部矩形共同表达初始内容，尚未成功完成的块不计入入口；图只在全部所需 texel 被入口和本图 copy 覆盖时允许完整读取，不借 initialized=true 扩大承诺

### Requirement: Typed CopyBlit 实际执行

CopyBlit SHALL 为 buffer copy 与 buffer-to-image upload 提供类型正确的 source/destination、拥有数据的 region 描述和真实 encoder emit。offset、pitch、mip、layer、extent 与后端对齐/格式约束 SHALL 被验证。其它公开 copy kind SHALL 有实现或显式 Unsupported，不得静默无操作。staging 与目标资源 SHALL 声明对应 RDG read/write 并由 slot lease 保活。

#### Scenario: Vertex index 与 texture 首次上传
- **WHEN** 图内包含 staging 到 vertex/index buffer，以及 staging 到指定 texture mip/layer 的 copy
- **THEN** compiled payload 不将 buffer 误解析成 image，执行真实 copy，后续 draw/readback 观察到预期数据并具备正确 barrier

#### Scenario: 非法 buffer copy 输入
- **WHEN** 输入含跨图 handle、溢出/越界范围、未定义读取，或实际设备/size/usage 与声明不符
- **THEN** Compile 返回明确错误，不产生可提交计划，也不遗留阻止该 lease 被 Discard 的借用

#### Scenario: DX12 紧密 image 数据与原生 footprint
- **WHEN** 图声明的完整 image copy 使用紧密 RGBA8 行，逻辑 buffer offset 不满足 DX12 原生 footprint 对齐
- **THEN** 后端在保持逻辑范围语义的前提下生成合法对齐 copy；若需要中间 backing，则保活到提交退休，不要求调用方猜测原生 row pitch，也不在 CPU 等待读回来重排

#### Scenario: 非零 mip/layer 的真实读回
- **WHEN** 在支持的 2D RGBA8 image 上，以非零 mip/layer 和紧密 buffer offset 上传并读回完整子资源
- **THEN** 原生 copy 使用对应 mip/layer、该 mip 的尺寸和 footprint；Vulkan 两种 compiler 与 DX12 读回一致，非法索引/范围显式失败，未支持的多 layer 单次 copy 显式 Unsupported；raster attachment 可显式选择单 mip/layer

#### Scenario: 局部矩形与显式 buffer 行距
- **WHEN** 单 sample RGBA8 的单 mip/layer copy 指定非零 origin、局部 extent，以及紧密或带 padding 的 buffer 行
- **THEN** Vulkan 两种 compiler 与 DX12 按相同区域执行上传/读回；pitch 单位为 texel，0 使用紧密默认，非零值不得小于 extent；非法范围/行距/地址溢出或超过内容分析预算在原生分配前失败

#### Scenario: 行间 padding 不参与内容依赖
- **WHEN** copy 的 buffer 行距大于有效行长度，只有每行实际像素 bytes 已初始化，末行之后没有额外 padding
- **THEN** source 读取只要求这些 bytes 有效，destination 只定义实际写入 bytes；其它 padding 保留原内容或未定义状态，不得因 copy 而允许导出未写 padding

#### Scenario: 矩形并集与覆盖前版本
- **WHEN** 多次局部写入的并集覆盖目标读取范围，读取前后又有相交矩形覆盖
- **THEN** 保留每次读取所需 producer；无消费者且被完全覆盖的旧写可裁剪，未覆盖空洞/其它 mip/layer/alias 旧内容不能补全读取，整图 export 和 sampled view 仍要求其全部内容有效

#### Scenario: DX12 首次部分写入的原生初始化
- **WHEN** 未初始化的 render-target backing 首次接受局部 upload
- **THEN** compiler 生成合法原生初始化和状态转换，encoder 不依赖未初始化 metadata；已有效内容的导入和后续矩形写入不得被 discard，原生初始化不扩大逻辑有效范围

### Requirement: 编译结果与提交状态发布分离

Compile SHALL 返回可检查的成功或诊断结果，不执行 acquire/present/submit。资源最终状态、upload receipts 与 GPU leases SHALL 作为候选提交产物输出；只有 Accepted submission SHALL 推进队列顺序状态账本，成功完成才推进 completed serial。重复 Compile 或丢弃未提交图 SHALL 不发布上传完成或资源全局状态。

#### Scenario: 编译两次后放弃
- **WHEN** 同一 graph 被重复 Compile，随后在 Submit 前放弃
- **THEN** 不额外 acquire，不产生成功上传通知，不推进资源的已提交最终状态，候选 leases 按未提交路径释放

#### Scenario: 实际录制回执与上传版本一致
- **WHEN** 后端已成功收齐全部 encode 任务及最终转换，交付 RecordedFrame
- **THEN** 回执保留对应不可变 graph 和实际录制的 live logical pass 集合；被剔除或编码失败的 copy 不能获得有效回执，common 不自行推测 native 执行结果

#### Scenario: 上传回执缺失或候选被覆写
- **WHEN** 上传批次的 copy 不在录制回执中，源/目标/范围/最终状态不匹配，或后续存活 pass 覆写该候选
- **THEN** 上传拥有者拒绝提交发布，保留可重试 CPU 数据；不能以 graphId 相同或 native buffer 创建成功代替精确内容证明

#### Scenario: 显式纳管的同队列跨图接续
- **WHEN** 第一图已 Accepted 但未完成，下一图在同一 context/graphics queue 导入同一已纳管 backing
- **THEN** 查询返回前图 accepted 最终状态及精确 token，下一图 initial 必须与之相符；不等 GPU 完成，不把候选或 completed 状态误作队列尾状态，不修改调用方 frozen graph

#### Scenario: 死资源与导出转换的账本参与
- **WHEN** 图声明包含被裁剪的已纳管资源，或只有存活导出的最终转换
- **THEN** 只有 native 编译计划实际使用的非 transient 导入形成候选；死声明不校验或发布新边界，导出转换成功提交后仍推进该资源状态

#### Scenario: 状态不替代内容证明
- **WHEN** 资源账本已记录 Accepted 或 completed 最终 layout/state，但导入未声明所读内容有效
- **THEN** 图仍按内容规则拒绝读取/export；不自动设置 initialized，也不因此发布资源 Resident

### Requirement: 后端完整编译与 native pass 构建

每个后端 SHALL 自行组织完整 Compile，涵盖内容分析/剔除、GPU 顺序、native pass scope 构建、lifetime/backing、bindings、同步、EncodeTaskGraph 和 SubmissionPlan。公共纯算法 MAY 复用，但后端接口 SHALL 不限于 CompileBarriers/公共 ExecutePasses 转调。合并/拆分 SHALL 保持 attachment LOAD/STORE/resolve、内容定义范围、顺序敏感 draws 和 Custom 的受限行为；Unsupported SHALL 明确报告。GPU 顺序变化后 SHALL 重新验证 lifetime/alias/同步。

#### Scenario: 合并 scope 影响同步
- **WHEN** 合并两个 raster passes 导致所需 barrier 不能放在合法位置
- **THEN** 后端拆开 scope 或选择经验证的合法同步方案，并重新验证资源计划，不静默删除 barrier

#### Scenario: 跨后端一致的内容剔除
- **WHEN** 图包含中间读、后续无用覆盖以及 UI LOAD
- **THEN** 各后端保留相同必要内容语义，可产生不同 native plans，不能因优化读取未定义内容

#### Scenario: 原生资源归属与真实描述校验
- **WHEN** 存活 pass 引用未知或其它 device 的 buffer/image/shader，或 image 声明与实际存储描述不一致
- **THEN** 后端在原生调用和具体类型转换前拒绝错误归属，在使用 image 前核验描述；编译失败不消费 lease，同一 lease 仍可编译合法图，不能只凭 backend 枚举认定同一 device

#### Scenario: DX12 DIRECT list 分片与稳定提交
- **WHEN** 一个受支持的 raster pass 被拆为多个 DIRECT list 任务，且任务完成顺序不同于声明顺序
- **THEN** 仅首片执行 load/clear，每片恢复所需图形状态，全部收齐后按编译顺序一次提交；copy、LOAD 与顺序敏感绘制的读回等同串行参考

#### Scenario: 没有 pass 的 image export 仍需转换
- **WHEN** 导出已初始化的 image，图内没有 live pass，但初始状态与所需最终状态不同
- **THEN** 后端生成必要的状态转换任务，不能以没有 pass 为由返回 NoWork

### Requirement: Vulkan 独立 compiler 与对应 encoder

Vulkan SHALL 提供 RenderPass 与 Dynamic Rendering 两套完整 compiler，各自构建 native scope、同步和匹配 PSO，并输出自身 encoder 程序。模式 SHALL 在 DeviceFrameContext 创建时确定并在该 context 生命周期内固定；Dispatch SHALL 仅接受匹配 context、lease 和模式的计划。公共纯分析、资源核验及底层命令辅助 MAY 复用；不能只在共享 encoder 的 Begin/End 中分支来代替两套 compiler。未支持的 attachment、binding、copy region 或并行模式 SHALL 明确报告，不得静默降为无操作。

#### Scenario: 同一图的两套 native 编译
- **WHEN** 同一受支持的 upload、CLEAR/LOAD/STORE raster 和 readback 图分别交给两种模式
- **THEN** RenderPass 计划使用 native render pass/subpass/framebuffer 和匹配的 PSO；Dynamic Rendering 计划使用 rendering scopes、显式 image barriers 和格式匹配的 PSO；两者读回同样内容

#### Scenario: 多 logical pass 的 scope 组织
- **WHEN** 相邻 raster passes 具备兼容 attachments 且存在经验证的合法合并与同步方案
- **THEN** RenderPass compiler 可将其映射为一个 native render pass 的多个 subpasses，Dynamic Rendering compiler 独立选择 scopes；两者保留 logical 映射和 LOAD/STORE 内容语义，不兼容或不能合法同步时拆分或返回明确错误

#### Scenario: 单 attachment 合并与中间 copy 对照
- **WHEN** 连续 raster 使用同一完整 color attachment，前项 STORE、后项 LOAD，且仅含受支持的普通绘制/混合
- **THEN** RenderPass 可使用首项 load/clear、末项 store 和有序 subpass 依赖合并；实际读回应与中间插入 copy 而拆分的参考路径一致，Dynamic Rendering 保持自己的 scope/barrier 方案

#### Scenario: CLEAR 和丢弃内容不被合并掩盖
- **WHEN** 存活序列中出现重新 CLEAR、attachment 切换、copy，或 STORE_DONT_CARE 后又 LOAD
- **THEN** 前三者形成合法的 scope 边界；丢弃内容后的读取在编译时被拒绝，不能靠省掉 store/load 使未定义内容伪装有效

#### Scenario: RenderPass 合并比较附件 view
- **WHEN** 连续 raster 使用同一 color/depth image，但任一附件选择不同 mip 或 layer
- **THEN** 不合并为同一 framebuffer scope；只有逻辑附件、各自 view 和 STORE/LOAD 连续性均匹配才可合并，各 scope 从所选 cell 的实际状态进入并更新该 cell

#### Scenario: 各后端保留每次 color resolve 的结果
- **WHEN** 连续 MSAA raster passes 分别声明 resolve，且后续消费者读取任一结果
- **THEN** Vulkan 两个 compiler 各自构建原生附件 resolve，RenderPass 当前不合并带 resolve 的 pass；DX12 在该 pass 全部 raster fragments 之后按顺序执行 resolve，源/目标使用正确子资源和状态，不能被后一次 resolve 替代；PSO 与 inheritance 的 samples 匹配 color storage

#### Scenario: 模式对应的 secondary inheritance
- **WHEN** 计划声明 secondary 编码任务
- **THEN** RenderPass 计划提供匹配 render pass、subpass 和 framebuffer 约束的 inheritance；Dynamic Rendering 计划提供匹配 formats、samples 和 view mask 的 rendering inheritance；不匹配在任务入队前被拒绝

#### Scenario: local read 能力不足
- **WHEN** 图的 attachment 局部读取语义依赖当前模式未启用或设备不支持的能力
- **THEN** compiler 仅可选择满足同等声明语义的合法拆分方案，否则返回 Unsupported，不能静默忽略读取或套用另一模式的能力

#### Scenario: shader local read 与普通 LOAD 区分
- **WHEN** 单 color raster 声明 colorLocalRead
- **THEN** 声明要求 LOAD 并保留输入内容依赖；缺少该模式的 local-read 编译/绑定能力时返回 Unsupported，普通 LOAD 或 blend 不自动要求 shader local read

#### Scenario: 后续 subpass 的 PSO 创建失败
- **WHEN** 同一个 scope 已创建 native 对象和前一个 subpass 的 PSO，而后续 PSO 创建失败
- **THEN** 返回带原生错误码的编译失败，释放已创建的 pipeline/framebuffer/render pass，不保留阻止 slot 被丢弃的计划借用

#### Scenario: 编译对象生命周期
- **WHEN** 外部 graph/shader/image 引用在 Compile 后释放，计划随后 Dispatch/Submit
- **THEN** pipeline、render pass、framebuffer 及其依赖保持有效至 GPU 完成；取消或编译失败及时销毁未提交对象，不遗留 slot 借用；device lost 使 context 停止接受新工作

### Requirement: CPU EncodeTaskGraph 与 GPU 执行计划分离

后端 SHALL 分别表达 GPU 访问顺序/同步和 CPU 编码任务依赖。编码 DAG SHALL 只添加 CPU 数据发布、native encoder 生命周期、独占录制资源及结果组装所需的边；GPU producer/consumer 依赖 SHALL 不自动变为 CPU 串行边。每个任务 SHALL 有稳定结果身份和 executor 约束。SubmissionPlan SHALL 按确定的 GPU 顺序组装结果，不能按 worker 完成次序提交。未声明可并行的 Custom callback SHALL 使用 coordinator 串行路径。

#### Scenario: 上传与消费者同时编码
- **WHEN** upload 与 draw 需要的 backing/bindings/CPU packets 已冻结且 native 录制互相独立
- **THEN** CPU 可同时编码两者，GPU 仍执行 copy 到 draw 的必要顺序和同步

#### Scenario: 顺序敏感 draw 的分块
- **WHEN** 同一有序 draw 队列的连续片段在不同 worker 编码且后段先完成
- **THEN** native 命令仍按原片段顺序组装；拆分不会重复 clear 或丢失前段结果

#### Scenario: 片段数多于实际 worker
- **WHEN** 一个有序 raster 队列切成多个连续 draw 片段，任务数大于 worker 数且后段先完成
- **THEN** 每段使用实际执行 worker 的独立 recording ordinal，按稳定 task index 保存结果；RenderPass/DR 各自的 primary scope 按原顺序执行所有 secondary，读回与 inline 参考一致

### Requirement: 后端编译计划身份与单次消费

BackendCompiledFrame SHALL 绑定 backend、device epoch、context identity、slot generation、GraphId 与 PlanId，由其后端管理 native payload。Dispatch SHALL 校验匹配并单次消费计划，保活计划/模块及资源到任务 drain。RecordedFrame SHALL 仅在全部任务退出且组装成功后产出，携带实际编码 receipts；仅匹配的 context 可将其单次提交。编译失败、取消和重复调用 SHALL 不推进全局资源状态。

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

