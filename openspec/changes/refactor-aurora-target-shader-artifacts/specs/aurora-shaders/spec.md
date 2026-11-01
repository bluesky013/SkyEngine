## ADDED Requirements

### Requirement: Binding plans validate native address mappings

Shader SHALL 为匹配的 stage artifacts 构建不可变 BindingPlan，验证资源语义、访问方式、stage、完整映射、native 容量及冲突。动态策略 SHALL 来自显式契约，不从 tier 推导。同一 pipeline 的同一 logical UBO 若要求无法共享单一 payload 的 stage 布局，本期路径 SHALL 明确 Unsupported，不丢弃另一 stage 布局继续执行。

#### Scenario: 原生槽位冲突
- **WHEN** 两个活跃资源占用同 stage/同原生命名空间的相同槽位
- **THEN** Shader/计划构建失败，不发布部分布局

#### Scenario: 不相容 stage block
- **WHEN** 同一 pipeline 中一个 logical UBO 的两个 stage 需要不同且当前单 range 无法表达的 block ABI
- **THEN** 链接明确 Unsupported，不保留第一份布局继续执行

### Requirement: Uniform plans and bindings preserve atomicity and lifetime

计划缓存 SHALL 使用链接布局/artifact 身份、CPU schema fingerprint 与 packing 协议版本。计划 SHALL 不持有每次 draw 的可变值；显式新版本 SHALL 与在途旧版本并存并沿现有 frame/Feature/GPU owner 退休。绑定装配 SHALL 验证全部活跃资源和字段后原子替换 draw；失败 SHALL 保留原 draw，优化掉的资源 SHALL 不要求绑定值。

#### Scenario: 失败不改变 draw
- **WHEN** 最后一个活跃字段或资源验证失败
- **THEN** 原 draw 的全部有效绑定不变，不发布部分 descriptor 或上传结果

#### Scenario: 两 View 与在途帧
- **WHEN** 相同逻辑接口在两个 View、多个 draw 和连续在途帧使用不同 CPU 值
- **THEN** 每次 GPU 使用读取相应快照，后续写入不覆盖旧值

#### Scenario: 计划版本变化
- **WHEN** CPU schema、目标 ABI 或 packing 协议改变，旧帧仍在使用旧 Shader
- **THEN** 新使用取得新计划，旧计划/artifact/字节保持有效至原有退休边界

### Requirement: Named updates preserve provider ownership and snapshots

命名事务 SHALL 维护显式 provider/assembler 归属，未知/歧义名、重复解析目标及字段与完整 block 重叠写 SHALL 失败。首次版本 SHALL 通过显式默认值或写入满足活跃必需参数；后续部分更新 SHALL 继承上一版本。图资源 SHALL 在 Core 同名桥接中声明访问并解析精确版本，再降为 RHI 操作；RHI SHALL NOT 接受 graph handle 或在编码时解析最新资产。已录制帧 SHALL 持不可变版本至 GPU 退休。

#### Scenario: 重叠写入拒绝
- **WHEN** 同一事务通过两个别名写同一字段，或同时写完整 block 和其字段
- **THEN** 明确冲突且不改变原版本，不按调用顺序选择最终值

#### Scenario: 后续部分更新
- **WHEN** 已完整初始化的 group 仅更新一个字段或一张纹理
- **THEN** 新版本保留其它值，旧在途帧继续使用原版本

#### Scenario: 名称不活跃
- **WHEN** 已声明的参数被当前 variant 优化掉
- **THEN** 写入返回可区分的 Inactive 状态且不创建 native binding，名字拼错仍是错误

#### Scenario: ResourceGroup 与图装配共用事务
- **WHEN** 相同 shader/interface 的 typed 参数分别经 ResourceGroup 和图装配更新
- **THEN** 两者直接复用相同的事务状态机和编码计划，得到相同目标 uniform 字节；各 materializer 只分配和发布一次，失败不改变既有版本

### Requirement: 发布产品保留三域变体选择

Shader 产品 SHALL 持久化 Vertex、Pipeline、Shader-local 归属、Vertex 语义条件与 Pipeline 预留位。无源码场景 SHALL 使用该元数据，缺失或冲突 SHALL 明确要求 recook。技术选择 SHALL 校验所有 stage 的声明并集，各 stage SHALL 保留自己的 schema/default；specialization 的命名有效值 SHALL 参与 DXIL 降级。

#### Scenario: 无源码选择非默认 Pipeline 与 Vertex
- **WHEN** 作者源码和编译器不可用，场景请求 SHADOWS 与具有颜色或实例流的 mesh
- **THEN** 使用已发布的归属和实际 Vertex 能力选择匹配产物，不以空 Pipeline 或硬编码 SKINNED 定义代替

#### Scenario: 跨文件 stage 与 specialization
- **WHEN** FS 独有一个默认开关或显式选择 specialization 值 4
- **THEN** FS 保留该声明和值，VS 不受 FS 独有宏污染，DXIL 不静默使用 0

### Requirement: 异步编译持有执行与输出所有者

异步 future SHALL 持有 compiler/module 与 cache root/writer 的有效 owner，SHALL 只接受一次执行；借用注册 SHALL NOT 被用于异步 miss。注销 SHALL 拒绝新任务，已接纳任务 SHALL 可安全完成并保持模块存活。

#### Scenario: 准备后注销并销毁 resolver
- **WHEN** future 准备后 compiler 注销且 resolver/source/output 的调用方引用释放
- **THEN** future 所持 owner 保活实际任务依赖，重复调度不重复编译或提交，最终释放后允许退休

### Requirement: Declaration identity is independent of target layout

共享导出 SHALL 提供稳定的接口身份、ResourceKey 与字段身份；编译 adapter SHALL 将源码声明与目标反射严格关联并验证唯一性。关联 SHALL 独立于短名字、声明顺序、物理槽位和其他目标 layout；同一组合接口中的重复 logical key SHALL 失败。

#### Scenario: 同名局部资源
- **WHEN** 不同 owner 或声明作用域使用相同局部资源名字
- **THEN** 各资源关联到各自声明身份；非法组合键冲突明确失败，不选第一个同名参数

#### Scenario: 身份不可恢复
- **WHEN** 请求目标中的活跃资源缺少唯一声明身份
- **THEN** 编译明确失败，不借第二目标或猜测补齐

### Requirement: Target reflection has explicit completeness

每个 stage 的反射 SHALL 显式携带协议版本、binding model、完整性、exact/conservative 使用分析标记与活跃资源集合。集合内每个资源 SHALL 有唯一有效、类型兼容的目标地址；错误 stage、孤立记录、重复/冲突地址、越界或未支持模型 SHALL 明确失败。metadata/地址查询失败 SHALL NOT 被解释成资源不活跃；合法零资源与反射缺失 SHALL 可区分。Metal 原生验收 SHALL 使用实际 pipeline reflection 核对可观测地址、活跃参数与 block ABI，并以 GPU 读回验证语义，shipping SHALL 不要求恢复完整 native reflection。

#### Scenario: 缺少一个活跃映射
- **WHEN** 反射列出活跃资源但没有该 stage 的目标地址
- **THEN** artifact 校验失败，不跳过该资源

#### Scenario: 不同原生命名空间
- **WHEN** Metal buffer(0)、texture(0)、sampler(0) 同时存在
- **THEN** 按类别分别验证；相同 stage/类别中的冲突仍被拒绝

#### Scenario: 手写 shader
- **WHEN** 手写 MSL 或其他后端 fixture 提交代码
- **THEN** 显式提供同协议的完整映射，不依赖空 nativeBindings 的连续编号规则

#### Scenario: 保守活跃集合
- **WHEN** 编译器明确产出保守使用集合
- **THEN** 原生验收证明其覆盖全部实际活跃资源，并为集合内每项提供有效地址；unknown 不能冒充 inactive

### Requirement: Stage artifact binds code reflection and identity

artifact SHALL 保存 target environment、producer/runtime ABI、entry/stage/variant、逻辑请求与接口身份、目标反射、显式 code format、精确代码长度及内容校验。ArtifactID SHALL 绑定这些规范化身份、精确代码字节与完整目标反射；CompileKey/发布请求 SHALL 映射到 ArtifactID。代码与反射 SHALL 共同发布、验证和保留所有权，不能独立替换。加载 SHALL 验证 envelope 与请求的一致性及内部结构；旧/损坏格式 SHALL 以明确错误要求重新 cook，不猜测转换。

#### Scenario: Blob 身份与 index 不一致
- **WHEN** index 指向可解码但 target、stage、entry 或接口身份不匹配的 blob
- **THEN** 加载拒绝该 blob，不能仅因路径正确而成功

#### Scenario: 代码或反射被替换
- **WHEN** artifact 的代码或反射内容与其校验身份不一致
- **THEN** 在创建 native function 或提交绘制前失败

#### Scenario: 精确代码长度
- **WHEN** MSL/DXIL 有效代码长度不是四字节倍数
- **THEN** 产物与消费入口仍使用精确长度，不将打包 padding 当作代码

### Requirement: Target environment and runtime compatibility are distinct

TargetEnvironment SHALL 明确 backend/code format、目标平台/API 下限、profile、所需能力与影响代码/布局的选项；ProducerIdentity SHALL 用于构建失效；RuntimeAbiVersion SHALL 用于产品加载兼容。Shipping SHALL 使用发布 manifest/index 中的生产者身份验证产物一致性，不要求当前编译器存在或版本相同。设备能力或 ABI 不兼容时 SHALL 返回明确错误且不隐式源码回退。

#### Scenario: macOS 与 iOS 产品
- **WHEN** 相同源程序分别面向 macOS 与 iOS Metal 环境
- **THEN** 目标环境与发布选择可区分，不仅以 MSL 格式枚举共用产品

#### Scenario: 无编译器 shipping
- **WHEN** 产品被搬迁且作者源码、Slang/DXC 不可用，或宿主安装了其他版本编译器
- **THEN** 兼容的已发布 artifact 仍可加载并完成原生读回

#### Scenario: 不兼容目标
- **WHEN** 设备不能满足 artifact 的能力/API 下限或 runtime 不理解其 ABI
- **THEN** 显式拒绝产品，不加载其他平台代码或自动编译源码

### Requirement: Logical parameter names survive compilation and shipping

ShaderInterface 与发布 artifact SHALL 保留供 Name-first 更新使用的 canonical 逻辑资源/字段名及稳定身份，独立于生成代码名字和 debug symbols；名字表内容 SHALL 纳入接口 fingerprint 与 ArtifactID。Shader 链接 SHALL 建立当前 interface/set 的不可变参数表；字段路径、显式别名和目标类型 SHALL 唯一且受检。序列化 SHALL NOT 保存进程内 sky::Name handle。缓存句柄 SHALL 绑定 interface/schema generation；未知名、歧义名和旧句柄 SHALL 明确失败，已声明但不活跃的名字 SHALL 有显式 Inactive 状态。

#### Scenario: 跨平台同名入口
- **WHEN** 三种后端对同一语义接口使用 Name 更新常量或资源
- **THEN** 名字解析到同一逻辑身份，各自 BindingPlan/UniformUploadPlan 决定原生位置与目标字节

#### Scenario: Shipping 去除调试信息
- **WHEN** 作者源码、Slang 与 debug symbols 均不存在
- **THEN** 已发布逻辑名字表仍支持合法 Name 更新，不读取生成代码恢复名字

#### Scenario: 参数句柄过期
- **WHEN** 将旧接口解析出的句柄用于不匹配的新接口
- **THEN** 明确失败，不因索引或名字恰好相同而写入另一个参数

## MODIFIED Requirements

### Requirement: Global tier 数据流

Global set 0 SHALL contain only values shared across the pipeline's views for the frame. View/camera values SHALL belong to the consuming Pass in set 1. Global uniform values SHALL use the selected shader artifact and UniformUploadPlan for checked target encoding, without inferring GPU layout from CPU sizeof. Production FrameGraph invocations SHALL retain frame-safe immutable values through the existing lease protocol.

#### Scenario: Multiple views
- **WHEN** main and shadow views use different cameras in one frame
- **THEN** they share frame-global values while retaining independent Pass camera snapshots

### Requirement: 跨后端 binding 索引一致性约定

Aurora SHALL 保证逻辑 ResourceKey 与资源语义跨后端一致；物理地址 SHALL 来自所选目标反射并允许不同。Vulkan SHALL 保留 descriptor set/binding，DX12 SHALL 保留 register class/space/register，Metal DirectArguments SHALL 保留 stage/category/index。logical set SHALL NOT 被强制解释为 Metal argument-buffer slot。调用方按逻辑键提供资源，由 Shader 派生的 BindingPlan 完成映射。

#### Scenario: 同一逻辑接口跨后端
- **WHEN** 同一语义接口使用各自目标产物绘制
- **THEN** shader 读取相同语义资源，即使各平台物理编号或 uniform 字节布局不同

#### Scenario: 同资源不同 stage 地址
- **WHEN** 一个逻辑 binding 在 Metal VS 与 FS 使用不同 index
- **THEN** 计划分别绑定两个原生地址，不重新按合并顺序编号

### Requirement: ResourceGroup::Update 批量写入资源

ResourceGroup SHALL 经同一 Name-first DescriptorEncoder 事务写入数据与资源，名称由 interface/set 参数表解析，并按计划校验类型、array element、资源类别、访问方式、device owner 与范围。错误 SHALL 通过受检结果显式报告，不能用断言、未定义行为或静默忽略表达可恢复错误。批量写入 SHALL 在全部验证与编码成功后发布不可变版本；失败 SHALL 保持先前有效绑定。多次成功事务 SHALL 允许部分更新，未覆盖的值继承先前版本，不修改仍被 GPU 使用的 descriptor/uniform。

#### Scenario: 写入 uniform 与 sampled image
- **WHEN** 对合法 uniform/image/sampler logical slots 批量提供同 device 的匹配资源
- **THEN** 发布后的绑定按计划在 native shader 中读取正确值

#### Scenario: 类型或 owner 不匹配
- **WHEN** 写入资源类型、访问方式或 GetDevice 与目标契约不匹配
- **THEN** 返回明确错误，原有有效绑定不变，不进入错误原生对象转换

### Requirement: DescriptorEncoder 接口

DescriptorEncoder SHALL 在现有接口中统一提供受检的 Name-first WriteData、WriteBlock、WriteBuffer、WriteImage、WriteSampler 与 End。WriteData SHALL 携带中性的语义类型/shape，WriteBlock SHALL 携带 CPU schema；SHALL NOT 仅凭裸字节数推断类型。名字解析后的内部操作 SHALL 复用 BindingPlan/UniformUploadPlan，各后端 SHALL NOT 各自搜索名字。 ResourceGroup 与 Core 图装配 SHALL 直接委托同一 RHI 中性参数事务前端，统一名字、类型、重复/重叠、inactive、必需参数与原子冻结；仅原生资源和图资源的 materializer 分离。图 materializer SHALL 保留显式访问声明与现有 lease，SHALL NOT 为复用事务而提前创建尚未分配的原生图资源。接口 SHALL NOT 恢复旧 ResourceUpdateInfo，SHALL NOT include Framework、MaterialPropertyValue、Aurora.Core 图或资产类型；本次事务暂存 SHALL 使用受检的类型化数据与所有权。

#### Scenario: 混合命名批量写入
- **WHEN** 一个事务按 Name 写入材质字段、纹理和 sampler 后 End
- **THEN** 同一逻辑参数表完成类型/归属检查并原子发布，GPU 同时读到本次值与资源

#### Scenario: 同大小异类型
- **WHEN** 将 uint32_t 写入声明为 FLOAT 的名字，即使两者大小相同
- **THEN** 明确类型错误，不能 memcpy 成功

### Requirement: ResourceGroup::CreateEncoder 返回后端 encoder

ResourceGroup SHALL 从其 Shader/interface/set 创建唯一作用域的 Name-first 更新事务，并显式接受所需 RHI 中性 updateContext。上下文 SHALL 由既有帧分配/编码路径提供 uniform range 与保活能力，不暴露 AssetManager、UUID 或 GraphImageHandle。缺少必需上下文或其 lease/device epoch 失效 SHALL 明确失败。任一写入失败 SHALL 使整个事务失效，即使忽略单项结果，End 也 SHALL 返回失败，不发布合法子集。End SHALL 验证并发布参数版本而非 queue Submit；未 End 的事务销毁 SHALL 取消暂存更新。

#### Scenario: 作用域受限
- **WHEN** 向 group 的事务写入另一个 set 或接口的参数句柄
- **THEN** 返回明确错误，不跨作用域搜索或推导 slot

#### Scenario: 更新不提交 GPU
- **WHEN** Name 数据事务成功 End
- **THEN** 仅经注入上下文形成可冻结版本，不创建 queue、等待 fence 或独立 Submit

#### Scenario: 忽略单项错误
- **WHEN** 一个 Name 写入失败后调用方仍执行其它合法写入并 End
- **THEN** End 返回失败，原版本不变，不能发布后续合法子集

### Requirement: Array binding 支持 count > 1

已经声明支持 descriptor arrays 的后端/执行路径 SHALL 按 `(binding,arrayElement)` 保存和编码元素，不能把 `binding+element` 当作另一个 logical binding。count SHALL 是受检的有界数量；未实现的数组/model SHALL 明确 Unsupported，不能把零/未知 count 降为 1。uniform 字段数组及 Texture2DArray 单资源 SHALL 与 descriptor arrays 区分，本 change SHALL 保留原有受支持能力。

#### Scenario: 已支持的 sampled image descriptor 数组
- **WHEN** 支持该能力的路径向 count=8 的 binding 写入 element 0..7
- **THEN** shader 的元素 3 读取第三号元素所写纹理，且不覆盖相邻 logical binding

#### Scenario: Metal DirectArguments 未支持数组
- **WHEN** 本期 Metal 通用 Pass 路径收到 descriptor array 或未知 count
- **THEN** 在发布计划前明确 Unsupported；合法 FLOAT4 uniform 数组和 Texture2DArray 单资源不因此被拒绝

### Requirement: 后端原生实现

各后端 DescriptorEncoder SHALL 消费 Shader 派生的 BindingPlan。Vulkan SHALL 保留既有持久 packed descriptor、dirty 更新与 update-template/显式写入路径；DX12 SHALL 按计划选择 descriptor table 或动态 root 参数，并保留只读/可写 buffer 的 SRV/UAV 区别；Metal DirectArguments SHALL 保存受检资源并按各 stage 的原生地址编码。未实现的 argument-buffer/model SHALL 明确失败，不以空实现成功返回。

#### Scenario: Vulkan 批量提交
- **WHEN** VulkanDescriptorEncoder 完成多个合法 Write 操作并提交
- **THEN** dirty 时通过同源 template 或已验证的显式更新发布全部描述符，非 dirty 时不重复写入

#### Scenario: DX12 动态绑定
- **WHEN** 动态 uniform 或 storage buffer 写入并绑定
- **THEN** 计划明确其 CBV/SRV/UAV root 类别以及 base/range/dynamic offset，不只凭 STORAGE_BUFFER 推断 UAV

#### Scenario: Metal 直接参数
- **WHEN** 一个 ResourceGroup 被用于 graphics 或 compute
- **THEN** 只在所属活跃 stage/category/index 绑定资源，不猜 logical set 对应的物理位置

### Requirement: RgBlockDesc 单一事实源

RgBlockDesc 等保留的桥接描述 SHALL 从权威共享 `.h` 的字段/资源列表派生逻辑语义，不能成为第二个可编辑 schema，也不能恢复 Slang-to-C++ 镜像生成。CPU schema SHALL 描述 C++ 表示，目标反射 SHALL 描述 GPU ABI；两侧通过 UniformUploadPlan 连接，不要求使用同一 offset 表。

#### Scenario: 同源桥接
- **WHEN** 一个共享声明同时被 C++、shader 导出与保留桥接接口消费
- **THEN** 字段与资源逻辑身份一致，原生 byte offset 仍分别由 CPU schema 与目标反射描述

#### Scenario: C++ layout 变化
- **WHEN** CPU padding/offset 改变而逻辑接口未变
- **THEN** CPU schema fingerprint 与旧 UniformUploadPlan 失效，不要求伪造目标 shader offset 或恢复镜像生成

### Requirement: dynamic UBO stable binding 契约

显式选择 dynamic UBO 的路径 SHALL 使用目标 block ABI 的固定逻辑范围，并使用 device capability 满足分配偏移及后端原生 view 对齐。该范围 SHALL NOT 统一按 std140 或 CPU sizeof 推导；原生 view 所需 padding SHALL 有足够 backing，且不能掩盖不足的逻辑范围。合法 pack buffer/布局版本不变时 SHALL 复用稳定 descriptor，逐 draw 以 dynamic offset 选择数据窗口，不要求逐对象改写 descriptor。零 range、越界或对齐错误 SHALL 显式失败，不使用断言/警告后继续或降为 whole-buffer range。

#### Scenario: 帧内稳定 descriptor
- **WHEN** 同一 pack buffer/布局版本提供多个合法 dynamic offsets
- **THEN** 各 draw 复用该版本 descriptor 并读取对应目标字节，范围与对齐来自实际目标和 device

#### Scenario: DYNAMIC 类型 range 为 0 拒绝
- **WHEN** dynamic uniform binding 提供零逻辑范围
- **THEN** 受检路径明确失败，先前有效绑定不变，不静默变成 whole-buffer view

### Requirement: pack writer 返回 dynamic offset

pipeline 的 typed uniform 写入 SHALL 先按实际 Shader 和 CPU schema 取得 UniformUploadPlan，再分配足够的目标 block 大小、编码目标字节并返回合法绑定偏移。分配对齐 SHALL 来自 device capability；逻辑绑定范围 SHALL 为实际目标范围而非 CPU sizeof 或分配 padding，native view 的额外对齐范围 SHALL 有足够 backing 且不能掩盖逻辑范围不足。动态策略 SHALL 显式选择，返回偏移 SHALL 通过既有绑定/执行路径使用。

#### Scenario: CPU 与目标大小不同
- **WHEN** 写入的 CPU 类型大小为 64 而合法目标 block 需要 80 字节
- **THEN** 分配并绑定目标所需范围，写入经计划转换的 80 字节，不复制或绑定越界

#### Scenario: Offset 直通执行
- **WHEN** 动态 uniform writer 返回的偏移被提交执行
- **THEN** executor 在相应逻辑 binding 的各原生 stage 地址使用该偏移，并校验顺序、对齐和范围

### Requirement: 四层绑定与图 storage 事务完整

资源 SHALL 使用 Global 0、Pass 1、Material 2、Batch 3。逐 draw 的 SceneSurface、palette、UI 图片和 sampler SHALL 归 Batch。带 frame context 的图事务 SHALL 保留 storage 的精确 range 和显式 provider，失败 SHALL 不改变原 draw；Metal SHALL 处理全部四个 set 与 raster storage。

#### Scenario: 骨骼 palette 通过生产事务
- **WHEN** 双在途骨骼 draw 经带 context 的 Apply 绑定只读结构化 palette
- **THEN** 每个 draw 使用自己的精确范围和姿态，错误 stride 或缺失资源明确拒绝并保留原绑定

#### Scenario: 第四组与 UI
- **WHEN** raster 或 compute 使用 Batch set 3，包括 UI 逐 draw 纹理
- **THEN** 原生 materializer 保留该 set 的资源，不截断为三组，也不将逐 draw 参数放入 Global/Material

### Requirement: Uniform upload plans encode target layout

UniformUploadPlan SHALL 校验共享语义、CPU 可读范围和目标 block ABI，并编码本期受支持的 scalar/vector/MAT4/FLOAT4 数组。计划 SHALL 处理 offset、size、array stride、matrix major/stride，清零目标 padding，不读取源 padding 或修改源对象。缺失/重复/未知字段、语义冲突、越界、重叠及算术溢出 SHALL 在发布前失败。仅当完整复制契约逐字段成立、复制范围的源与目标全部由声明值字节覆盖且不含 padding 时 SHALL 使用整块复制快路；其他情况 SHALL 分段编码并清零 padding。

#### Scenario: 不同 offset 和 stride
- **WHEN** CPU 字段与目标字段位置或数组 stride 不同
- **THEN** 按目标位置编码，目标 padding 为零，源对象和边界哨兵保持不变

#### Scenario: 非对称矩阵
- **WHEN** CPU MAT4 与目标矩阵表示不同
- **THEN** 编码后 shader 对非对称矩阵的实际运算与 CPU 语义一致，不以 identity matrix 作为唯一证据

#### Scenario: 快路与通用路径等价
- **WHEN** 两条路径均适用于同一兼容 ABI 和输入值
- **THEN** 得到相同有效目标字节；总大小相同但成员布局不同不启用整块复制

### Requirement: ResourceGroup 布局等于 shader reflection 的完整 set

ResourceGroup SHALL cover the selected Shader's complete active logical set through its BindingPlan. Name-first transactions SHALL validate all required active resources/fields before publishing the first complete parameter version. Subsequent partial updates SHALL inherit prior values. Missing required inputs SHALL NOT be treated as valid null descriptors; declared inactive keys SHALL remain distinguishable from unknown names.

#### Scenario: Required binding missing
- **WHEN** the first transaction supplies only a uniform while an active image is also required
- **THEN** End fails without publishing a usable partial version

#### Scenario: Later partial update
- **WHEN** an already complete group updates only its uniform
- **THEN** the new version retains the previous image while old in-flight versions remain unchanged

### Requirement: shader 生命周期不因 ResourceGroup 延长

Shader-derived native layout and binding plans SHALL remain valid while pipelines, ResourceGroups or frozen parameter snapshots use them. Releasing the caller's Shader reference SHALL NOT invalidate these borrowers. Shared ownership MAY retain the Shader or an independently owned immutable layout; no caller-managed delayed release SHALL be required. Frame/GPU resource lifetime SHALL still follow the normal retirement protocol.

#### Scenario: Caller releases Shader
- **WHEN** a pipeline and ResourceGroup remain after the caller releases its Shader reference
- **THEN** their layout and parameter mappings remain usable until their final borrower retires

### Requirement: 后端在 Shader 内派生 native layout

各后端 SHALL 在现有 Shader 内由经过身份与完整性校验的目标 stage artifacts 派生 native layout 和 BindingPlan。Vulkan SHALL 派生 descriptor set layouts 与 VkPipelineLayout；DX12 SHALL 派生 root signature；Metal DirectArguments SHALL 派生逐 stage 的 buffer/texture/sampler 地址表。系统 SHALL NOT 恢复可由调用方独立传入的 PipelineLayout 对象，也 SHALL NOT 强制把 logical set 解释成 Metal argument-buffer slot。

#### Scenario: Pipeline 使用 shader 布局
- **WHEN** 创建 graphics 或 compute pipeline
- **THEN** pipeline 使用该 Shader 从匹配 artifacts 派生的 native layout，不接受独立布局覆盖

### Requirement: shader 派生的 set layout 供 ResourceGroup 复用

ResourceGroup SHALL 从同一 Shader 的受检 BindingPlan 获取相应 logical set 的布局。Vulkan SHALL 复用该 Shader 的真实 set 索引对应 VkDescriptorSetLayout；DX12 SHALL 使用计划中的 table/root 映射；Metal SHALL 使用计划中的逐 stage DirectArguments 地址。未实现的 binding model SHALL 返回显式 Unsupported。

#### Scenario: Vulkan set 索引有空洞
- **WHEN** Shader 的 logical sets 为 0 与 2
- **THEN** ResourceGroup 分别取得对应真实 set 的布局，不因容器下标而错位

#### Scenario: ResourceGroup 与 pipeline 同源
- **WHEN** 用同一 Shader 创建 pipeline 和 ResourceGroup
- **THEN** 两者消费同一个不可变 BindingPlan 及其 native layout

### Requirement: shader 反射进入 RHI Shader

RHI SHALL 定义不依赖 Slang 或平台原生头的逻辑接口、目标反射及 artifact 视图契约。编译器 SHALL 填充该契约；ShaderFunction SHALL 保留自身 stage artifact 的身份与不可变数据所有权；Shader SHALL 从匹配的 stage artifacts 链接反射及绑定计划。调用方 SHALL NOT 为无关代码另配一份反射绕过验证，后端 SHALL NOT 反向依赖编译器实现。

#### Scenario: 后端消费已编译产物
- **WHEN** runtime 只获得已发布 artifact，没有 Slang 库或作者源码
- **THEN** RHI 能检查并消费其中的代码、逻辑接口与目标 ABI

#### Scenario: 函数与反射错配
- **WHEN** 调用方组合属于不同 artifact 的 stage code 与 reflection
- **THEN** Shader 创建在 native 执行前明确失败

### Requirement: Vulkan layout 由反射派生

VulkanShader SHALL 从经过校验的 stage artifacts 和其链接 BindingPlan 构建 VkDescriptorSetLayout 与 VkPipelineLayout，按实际目标 descriptor set/binding、type/count/stage 可见性及已支持 push constants 派生。布局 SHALL 与实际 Shader 代码同源，不接受调用方另配未绑定 artifact 身份的反射。

#### Scenario: 反射驱动 descriptor set layout
- **WHEN** 匹配的 artifacts 包含 set 0 的 UBO 与 set 1 的 sampled image/sampler
- **THEN** VulkanShader 派生对应 set layouts 和 pipeline layout，类型、数量与实际活跃 stage 一致

### Requirement: DX12 root signature 由反射派生

D3D12Shader SHALL 从经过校验的目标反射派生 root signature，保留 register class、space、register、count 与 stage 信息；逻辑 set/binding SHALL 经 BindingPlan 映射。只读 structured/raw buffer SHALL 使用 SRV，可写变体 SHALL 使用 UAV，动态 buffer 的 root 参数选择 SHALL 同样遵守访问方式；descriptor 创建/写入 SHALL 保留 raw/structured 形态与 element stride。Graphics/Compute pipeline SHALL 使用该 Shader 的 root signature，不接受外部 root signature 参数。

#### Scenario: PSO 使用 shader 的 root signature
- **WHEN** 创建 D3D12 graphics 或 compute pipeline
- **THEN** pRootSignature 来自相应 Shader 的受检目标反射

#### Scenario: 只读输入与可写输出
- **WHEN** compute 同时声明 StructuredBuffer 或 ByteAddressBuffer 输入与对应 RW 输出
- **THEN** root/table 与 descriptor 编码分别使用 SRV 和 UAV，GPU 输出包含正确输入值

### Requirement: shader reflection 的 null 与 empty 语义

Shader 创建 SHALL 要求完整、非缺失的 stage artifact 与目标反射。显式标为完整且活跃资源集合为空的反射 SHALL 合法；缺失反射、未知完整性或错误 stage SHALL 明确失败。零资源 SHALL NOT 表示顺序槽位兼容模式；不存在的 logical set SHALL 不能创建 ResourceGroup。

#### Scenario: 缺失反射拒绝
- **WHEN** stage code 没有匹配的完整目标反射
- **THEN** Shader 创建失败，不按资源顺序补出地址

#### Scenario: 零资源合法
- **WHEN** 合法 artifact 显式携带完整的零资源反射
- **THEN** Shader 可创建，针对不存在 set 的 ResourceGroup 创建失败

### Requirement: ResourceGroup 复用 shader 派生的 native set layout

各后端 ResourceGroup SHALL 复用同一 Shader 内已派生的 BindingPlan 与 native layout，不自建另一套映射。Vulkan SHALL 用对应 set 的 VkDescriptorSetLayout 分配；DX12 SHALL 按计划分配并编码 table/root 资源；Metal DirectArguments SHALL 按计划保存并绑定逐 stage 参数。其所有权 SHALL 保证 ResourceGroup 使用期间计划与 native layout 有效。

#### Scenario: Vulkan descriptor 与 pipeline 同源
- **WHEN** 使用同一 Shader 的 ResourceGroup 执行绘制
- **THEN** descriptor set 与 pipeline 使用同源 VkDescriptorSetLayout

#### Scenario: Metal stage 地址不同
- **WHEN** 一个逻辑 uniform 分别映射为 VS buffer(2) 与 FS buffer(5)
- **THEN** ResourceGroup 在两个 stage 使用各自记录的地址与本次 draw 的数据偏移

### Requirement: Pass-owned shared header source

每个采用共享声明的 Global/Pass/Material/Batch 接口 SHALL 有一个权威共享 `.h`，直接供 C++ 消费，并由既有工具导出持久、版本化 `.slangh` 供 shader 消费。相同字段/资源列表 SHALL 产生语义接口、typed logical slots 与 CPU schema；CPU sizeof/offsetof SHALL 仅描述 CPU 表示，不要求等于所有目标 ABI。Global/Pass/Material/Batch SHALL 分别使用 logical sets 0/1/2/3，View SHALL 按需归 Pass。

#### Scenario: C++ 无 shader 生成前置
- **WHEN** 编译消费共享声明的 C++ target
- **THEN** 无需执行 Slang、ShaderHeaderTool 或目标平台生成器即可取得 C++ 类型和 CPU schema

#### Scenario: 共享源变更
- **WHEN** 修改权威声明或其导出 helper
- **THEN** 相关 C++、持久 shader export 与 shader 依赖身份按各自输入更新，不生成供 C++ 消费的镜像 ABI header

#### Scenario: 目标布局不同
- **WHEN** 同一平坦 uniform 接口的 CPU 与 SPIR-V、DXIL 或 MSL 目标布局不同
- **THEN** 语义兼容的受支持字段通过 UniformUploadPlan 编码，不因合法 offset/stride 差异而拒绝

### Requirement: 类型映射

共享声明 SHALL 支持 FLOAT/INT/UINT、FLOAT2/FLOAT4、MAT4 和有界 FLOAT4 uniform 数组，保留逻辑字段类型、元素数量及 CPU 表示。目标反射 SHALL 独立描述其实际存储布局和矩阵表示。BOOL/FLOAT3/half、嵌套结构、未支持矩阵或数组、未支持的 storage-buffer 形态与 descriptor arrays SHALL 明确失败，不能遗漏字段。共享声明 SHALL 支持用于 palette 的只读 STRUCTURED/FLOAT4 buffer，并保留访问方式及 element stride。资源-only 声明 SHALL 保留形状及 typed slots，不引入占位 UBO；描述一种资源 SHALL NOT 意味着该执行路径已支持它。

#### Scenario: 有界 uniform 数组
- **WHEN** 共享接口声明 FLOAT4 数组
- **THEN** CPU 与目标各自记录 count/stride，上传计划按元素编码；合法的 stride 差异不要求两侧原始字节相同

#### Scenario: Resource-only 声明
- **WHEN** 接口只包含受支持的 texture/sampler 资源
- **THEN** 产生资源语义与 typed slots，不产生占位常量块

#### Scenario: 不支持字段拒绝
- **WHEN** 声明包含本期不支持的字段类型
- **THEN** 导出或验证明确失败，不能发布缺字段的接口或计划

### Requirement: 三方一致性校验

系统 SHALL 分别验证共享语义、CPU schema 与每个目标 ABI，验证上传后的目标字节与语义值一致。CPU schema SHALL 使用真实 sizeof/offsetof；目标 layout SHALL 来自请求目标自身；RgBlockDesc 等桥接描述 SHALL 继续从同一字段列表派生，不成为独立事实源。合法布局差异 SHALL 由 UniformUploadPlan 处理，语义冲突或不能编码的布局 SHALL 在修改 draw/执行 GPU 前明确失败。各目标编译反射测试 SHALL NOT 代替对应平台原生执行。

#### Scenario: 语义漂移被拒绝
- **WHEN** 目标字段的逻辑类型、数量或含义与共享接口不兼容
- **THEN** 计划构建或装配明确失败，原 draw 不变

#### Scenario: 不同 packing 保持值一致
- **WHEN** CPU schema 与目标 ABI 的 offset、数组 stride 或矩阵表示不同且处于支持集合
- **THEN** 编码后 shader 读到与 CPU 语义相同的值

#### Scenario: 交叉构建
- **WHEN** 移动目标编译共享 C++ 声明
- **THEN** 无需执行目标生成器；交叉构建结果与真机 shader 执行证据分开记录

### Requirement: 反射是编译产物

每次成功编译 SHALL 原子地产出请求目标的代码与完整目标反射，二者 SHALL 作为同一 artifact 缓存/发布。宏变体 SHALL 可以改变逻辑活跃集合或布局；原生 specialization 参数 SHALL 遵守既有不改变接口布局的约束，并进入相应 pipeline 选择。反射内容摘要 SHALL 用于产物完整性与计划身份，SHALL NOT 循环成为产生自身的编译输入 key。

#### Scenario: 宏变体改变活跃资源
- **WHEN** 宏变体增删一个资源
- **THEN** 各变体 artifact 分别保存自己的受检活跃集合、代码和目标映射

#### Scenario: 编译或反射失败
- **WHEN** 代码生成成功但所需反射或 metadata 验证失败
- **THEN** 编译整体失败，不发布只有代码的可消费结果

### Requirement: 统一 cache key 与 ShaderCache 接口

开发/cook 缓存 SHALL 用规范化源码依赖、entry/stage、变体与 schema/interface、TargetEnvironment、实际 ProducerIdentity 计算 CompileKey。ProducerIdentity SHALL 包含参与工具链与反射转换规则身份，生产调用 SHALL NOT 用零占位。ShaderCache SHALL 保持既有 offline/local root 职责；同一请求输入 SHALL 稳定命中，不同目标环境或生产者 SHALL 失效。Shipping SHALL 从发布 index 解析 artifact，不依赖当前安装编译器重算 CompileKey。

#### Scenario: 统一且稳定的 key
- **WHEN** 规范化输入相同，仅构建机器绝对路径不同
- **THEN** CompileKey 相同

#### Scenario: 工具链或布局模式变化
- **WHEN** 编译器、profile、平台下限、影响布局的选项或反射转换规则改变
- **THEN** CompileKey 改变，旧开发产物不能被当作新请求命中

### Requirement: Slang 编译通道

ShaderCompilerSlang SHALL 使用既有 Slang Compilation API 直接生成请求的 SPIR-V、MSL 或 DXIL 代码，并仅从该目标的 layout/metadata 生成 ABI 反射。逻辑身份 SHALL 来自同源声明身份通道，SHALL NOT 借另一个目标的 layout、短名字 first-match、资源顺序或生成文本解析恢复。虚拟 include SHALL 继续通过既有 ShaderFileSystem 及持久导出接口解析，保留源码 snapshot 与 owner/dependency 约束。

#### Scenario: SPIR-V 直接产物
- **WHEN** 请求 SPIR-V 的平坦 ConstantBuffer 程序
- **THEN** 得到非空 SPIR-V、声明逻辑键与该目标实际 ABI，不请求 MSL/DXIL layout

#### Scenario: MSL 单目标产物
- **WHEN** 请求 MSL stage
- **THEN** session 仅包含所请求的 MSL target，反射保留该目标原生类别/槽位和真实 block 布局，不创建辅助 SPIR-V layout

#### Scenario: 虚拟 include
- **WHEN** 程序 include 已注册的 owner `.slangh`
- **THEN** 经同一受检源码视图解析，无需访问作者 C++ 头或恢复已移除的嵌入 shader 链路

### Requirement: Shader 使用收集（离线预编）

Usage collection SHALL be explicitly enabled by the host and record source/entry/stage/target environment plus complete typed selection. Persistent identity SHALL NOT rely only on a 128-bit compatibility key or hash. Flush SHALL union compatible records across sessions with checked format/version handling. Offline cook SHALL consume explicit requested selections, project each stage's dependencies and publish the allowed complete selections; it SHALL NOT infer an unrestricted Cartesian product or replace nondefault values with defaults.

#### Scenario: Complete selection survives collection
- **WHEN** V/P/S inputs include nondefault bool, signed and unsigned values
- **THEN** usage/cook retain all typed values and their stage-specific meaning

#### Scenario: Collection disabled
- **WHEN** the host did not enable usage collection
- **THEN** resolution does not implicitly publish a usage file

#### Scenario: Session union
- **WHEN** two enabled sessions collect distinct valid selections
- **THEN** the published usage union retains both without duplicate identities

### Requirement: 编译器抽象与可选模块

IShaderCompiler and its shared factory SHALL keep the compiler optional for PRODUCTS consumption. DEVELOPMENT SHALL require the actual producer identity and valid sources, compiling only a cache miss. Runtime-only compiler registration SHALL be revoked on shutdown; accepted asynchronous work SHALL retain its compiler and cache endpoint owners until completion; defining DLL code SHALL remain process resident. Missing compiler ownership for an async miss SHALL fail explicitly.

#### Scenario: Products without compiler
- **WHEN** no compiler module is loaded and PRODUCTS has a compatible published selection
- **THEN** the artifact loads without reconstructing a compiler identity

#### Scenario: Async task survives unregister
- **WHEN** a compiler is unregistered after a task was accepted
- **THEN** that task retains its owned compiler and cache endpoints until completion without a per-object code token

### Requirement: 单写者 IO 与原子落盘

Cache publication SHALL serialize index changes and atomically replace temporary files only after successful encoding and write. ShaderCacheWriter SHALL provide queued single-writer operation; synchronous commits SHALL use the existing guarded store. An unsupported or failed rename SHALL return failure rather than fall back to exposing a partial direct write. Async jobs SHALL retain owned compiler and cache endpoints until completion; errors SHALL remain observable through commit/flush results.

#### Scenario: Concurrent submissions
- **WHEN** several jobs publish different artifacts through the writer
- **THEN** successful flush retains all committed records

#### Scenario: Rename fails
- **WHEN** final atomic replacement fails
- **THEN** publication reports failure and the prior published index remains readable

### Requirement: 编译键与 entry 级产物

Compile identity SHALL include normalized source/dependency closure, entry/stage, code-affecting selection, schema/interface, target environment and actual producer identity. Complete logical selection identity SHALL remain distinct from stage code and native executable identity. Compatible native-specialization selections MAY share code, but SHALL retain independent stage-specific values and executable selection. Artifact identity SHALL bind exact code length and target reflection; reflection output SHALL NOT become a circular compile input.

#### Scenario: Native selections share code
- **WHEN** two complete selections differ only in a supported native specialization value
- **THEN** code may be reused while executable identity and stage values remain distinct

#### Scenario: Dependency changes
- **WHEN** an included export, companion, target option or compiler adapter changes
- **THEN** affected compile identities invalidate old development results

### Requirement: 内容寻址缓存（单 index + blobs）

Each target directory SHALL contain a versioned index and content-addressed stage artifacts. The current cache format SHALL preserve complete logical schema, allowed selections, code identities, target environment and producer metadata; code plus reflection SHALL be checked as one ArtifactID. Offline and local roots SHALL retain explicit read/write roles. Packaging SHALL combine selected owners' records with conflict validation instead of first-match mounting that hides another index. Incompatible formats SHALL require recook.

#### Scenario: Multiple shader owners
- **WHEN** two selected owners contribute distinct requests for the same target
- **THEN** the distributed index retains both; conflicting request identities fail

#### Scenario: Artifact corruption
- **WHEN** code or reflection differs from the recorded ArtifactID
- **THEN** loading rejects the artifact before native creation

### Requirement: 解析优先级 离线 > 源码 > 本地实时

Resolution SHALL explicitly distinguish DEVELOPMENT and PRODUCTS. DEVELOPMENT SHALL require the current source closure and actual producer identity, validate matching offline/local cache artifacts and compile only on a valid miss. PRODUCTS SHALL resolve complete published selections from its selected product root without source or compiler access. Missing, corrupt, incompatible or ambiguous products SHALL fail explicitly; neither mode SHALL accept a blob merely because its file exists.

#### Scenario: Development cache hit
- **WHEN** source, producer, target environment and normalized selection match a validated artifact
- **THEN** development loads that artifact without recompilation

#### Scenario: Products without compiler
- **WHEN** a relocated product distribution contains the requested complete selection and compatible runtime ABI
- **THEN** resolution succeeds without sources or the producer toolchain

#### Scenario: Unpublished selection
- **WHEN** PRODUCTS receives a selection absent from its manifest/index
- **THEN** it reports missing product without fallback compilation or substituting defaults

### Requirement: 三段解析（schema / 变体 / 二进制）

Preparation SHALL obtain complete logical schema, normalize typed V/P/S values, project stage dependencies and resolve matching stage artifacts. Schema fingerprint, ownership, ranges and defaults SHALL be validated before lookup. A supplied hash SHALL NOT replace full canonical selection validation. Immutable preparation SHALL retain program, values, source/product metadata and owners across asynchronous execution.

#### Scenario: Supplied fingerprint is stale
- **WHEN** request schema or selected values disagree with their metadata
- **THEN** preparation fails instead of resolving by a caller-supplied hash alone

### Requirement: Shader 身份为相对路径（非资产）

ShaderRef SHALL identify an explicit source path and entry without becoming a UUID asset. Paths SHALL be canonical domain-relative paths in validated owner inventories, with normalized separators and rejected traversal. Spelling SHALL be retained; case-only path collisions SHALL fail rather than being silently lowercased. Source and relocated product resolution SHALL preserve the same identity.

#### Scenario: Portable identity
- **WHEN** a module's physical root changes but its logical shader path and entry stay the same
- **THEN** requests retain the same logical identity

#### Scenario: Case-only collision
- **WHEN** two owners expose paths differing only by case
- **THEN** collection rejects the conflict before publishing the inventory
