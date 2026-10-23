## MODIFIED Requirements

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

每个 Global/Pass/Batch 接口 SHALL 有一个权威共享 `.h`，直接供 C++ 消费，并由既有工具导出持久、版本化 `.slangh` 供 shader 消费。相同字段/资源列表 SHALL 产生语义接口、typed logical slots 与 CPU schema；CPU sizeof/offsetof SHALL 仅描述 CPU 表示，不要求等于所有目标 ABI。Global/Pass/Batch SHALL 保持 logical sets 0/1/2，View SHALL 按需归 Pass。

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

共享声明 SHALL 支持 FLOAT/INT/UINT、FLOAT2/FLOAT4、MAT4 和有界 FLOAT4 uniform 数组，保留逻辑字段类型、元素数量及 CPU 表示。目标反射 SHALL 独立描述其实际存储布局和矩阵表示。BOOL/FLOAT3/half、嵌套结构、未支持矩阵或数组、共享 storage-buffer 声明与 descriptor arrays SHALL 明确失败，不能遗漏字段。资源-only 声明 SHALL 保留形状及 typed slots，不引入占位 UBO；描述一种资源 SHALL NOT 意味着该执行路径已支持它。

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

## ADDED Requirements

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
