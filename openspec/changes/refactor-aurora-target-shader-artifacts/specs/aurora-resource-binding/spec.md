## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: Binding plans validate native address mappings

Shader SHALL 为匹配的 stage artifacts 构建不可变 BindingPlan，验证资源语义、访问方式、stage、完整映射、native 容量及冲突。动态策略 SHALL 来自显式契约，不从 tier 推导。同一 pipeline 的同一 logical UBO 若要求无法共享单一 payload 的 stage 布局，本期路径 SHALL 明确 Unsupported，不丢弃另一 stage 布局继续执行。

#### Scenario: 原生槽位冲突
- **WHEN** 两个活跃资源占用同 stage/同原生命名空间的相同槽位
- **THEN** Shader/计划构建失败，不发布部分布局

#### Scenario: 不相容 stage block
- **WHEN** 同一 pipeline 中一个 logical UBO 的两个 stage 需要不同且当前单 range 无法表达的 block ABI
- **THEN** 链接明确 Unsupported，不保留第一份布局继续执行

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

### Requirement: Uniform plans and bindings preserve atomicity and lifetime

计划缓存 SHALL 使用链接布局/artifact 身份、CPU schema fingerprint 与 packing 协议版本。计划 SHALL 不持有每次 draw 的可变值；显式新版本 SHALL 与在途旧版本并存并沿现有 frame/module/GPU lease 退休。绑定装配 SHALL 验证全部活跃资源和字段后原子替换 draw；失败 SHALL 保留原 draw，优化掉的资源 SHALL 不要求绑定值。

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

### Requirement: Global tier 数据流

Global set 0 SHALL contain only values shared across the pipeline's views for the frame. View/camera values SHALL belong to the consuming Pass in set 1. Global uniform values SHALL use the selected shader artifact and UniformUploadPlan for checked target encoding, without inferring GPU layout from CPU sizeof. Production FrameGraph invocations SHALL retain frame-safe immutable values through the existing lease protocol.

#### Scenario: Multiple views
- **WHEN** main and shadow views use different cameras in one frame
- **THEN** they share frame-global values while retaining independent Pass camera snapshots
