# aurora-shader Specification

## Purpose
TBD - consolidated from: aurora-shader-derived-layout aurora-shader-derived-pso aurora-shader-derived-resource-group aurora-shader-header-codegen aurora-shader-variant shader-slang-spike
## Requirements
### Requirement: PipelineLayout 接口对象被移除

RHI 接口层 SHALL 不暴露 `PipelineLayout` 对象：不存在 `aurora/rhi/PipelineLayout.h`，`Device` SHALL 无 `CreatePipelineLayout`，`GraphicsPipeline::Descriptor` 与 `ComputePipeline::Descriptor` SHALL 无 `layout` 字段。

#### Scenario: 无 PipelineLayout 符号

- **WHEN** 调用方 include `<aurora/rhi/PipelineLayout.h>` 或 `device->CreatePipelineLayout({})`
- **THEN** 编译失败（符号已删除）

#### Scenario: pipeline 描述符无 layout

- **WHEN** 构造 `GraphicsPipeline::Descriptor{}` / `ComputePipeline::Descriptor{}`
- **THEN** 描述符不含 layout 字段；pipeline 创建只依赖 shader

### Requirement: 后端在 Shader 内派生 native layout

各后端 SHALL 在现有 Shader 内由经过身份与完整性校验的目标 stage artifacts 派生 native layout 和 BindingPlan。Vulkan SHALL 派生 descriptor set layouts 与 VkPipelineLayout；DX12 SHALL 派生 root signature；Metal DirectArguments SHALL 派生逐 stage 的 buffer/texture/sampler 地址表。系统 SHALL NOT 恢复可由调用方独立传入的 PipelineLayout 对象，也 SHALL NOT 强制把 logical set 解释成 Metal argument-buffer slot。

#### Scenario: Pipeline 使用 shader 布局
- **WHEN** 创建 graphics 或 compute pipeline
- **THEN** pipeline 使用该 Shader 从匹配 artifacts 派生的 native layout，不接受独立布局覆盖

### Requirement: push constants 归入 shader 反射

`PipelineLayout::Descriptor::pushConstants` SHALL 被删除；push-constant 区间（offset/size/stages）SHALL 由 shader 声明并经反射得到，与 descriptor set/binding 同源。

#### Scenario: 无 PipelineLayout push constants

- **WHEN** 尝试在 layout 描述符里声明 push constants
- **THEN** 无此字段（编译失败）；push constants 改由 shader 反射承载

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

### Requirement: ResourceGroup 由 shader + set 创建

`ResourceGroup::Descriptor` SHALL 含 `Shader *shader` 与 `uint32_t set`，不再含 `ResourceGroupLayout *layout`。`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 从 shader 反射派生该 set 的 binding 布局并分配 ResourceGroup。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。

#### Scenario: 用 shader 创建 ResourceGroup

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`，其 binding 布局与 shader reflection 的 set 0 一致

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr，且 logger 报错

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

### Requirement: UBO 类型按显式绑定声明
Shared declarations SHALL describe the base uniform type. The consuming Pass/material SHALL explicitly select static or dynamic binding and keep shader/layout type, allocation and binding offsets consistent; set numbers SHALL NOT determine this policy.

#### Scenario: Explicit dynamic binding
- **WHEN** a consumer selects a dynamic uniform binding
- **THEN** its shader layout, allocation and supplied offsets agree; a policy mismatch returns an explicit error before drawing

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

### Requirement: 虚拟 include
`ShaderCompilerSlang` SHALL resolve quoted and angle includes through `ShaderFileSystem`, including persisted shader-only .slangh exports and their common helper, without requiring disk copies or native host header generators.

#### Scenario: Packaged scene shaders
- **WHEN** only the built-in scene virtual files are registered, without filesystem search paths
- **THEN** SceneForward, SceneLit, SceneShadow and SceneToneMap compile for each supported target with their shared declaration includes

#### Scenario: Shared helper cache invalidation
- **WHEN** a shared helper reached through an angle include changes
- **THEN** the source closure hash changes and old cache entries are rejected; quoted includes search the including directory before the logical root

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

### Requirement: 统一变体模型

Aurora SHALL normalize one typed key/value selection from Vertex, Pipeline and Shader-local inputs. Material, custom Batch, Fullscreen and Compute callers SHALL provide the Shader-local domain without requiring a Material object. Each logical key SHALL have one owner and a consistent type/range/default across stages. Unknown, duplicate, wrong-owner and invalid values SHALL fail explicitly. Omitted and explicit defaults SHALL normalize identically. Stage projection SHALL preserve only that stage's declared dependencies.

#### Scenario: Pass without Mesh
- **WHEN** fullscreen or compute supplies Pipeline and Shader-local inputs without Mesh input
- **THEN** it uses the same selector and validation as a scene request

#### Scenario: Duplicate or cross-domain input
- **WHEN** providers repeat a key or write a key owned by another domain
- **THEN** selection fails without order-dependent overriding

### Requirement: bitmask key 与数据驱动 schema

ShaderVariantSchema SHALL describe typed logical keys, ownership, defaults and ranges independently of the legacy 128-bit ShaderVariantKey. Full selections SHALL use canonical typed key/value identities and support the schema's checked limits (1024 entries and 32768 logical bits). The retained bitmask helper SHALL reject values or layouts it cannot represent; it SHALL NOT limit production selection to 128 bits or truncate signed values.

#### Scenario: Complete selection exceeds the legacy key
- **WHEN** a valid typed selection needs more than 128 bits
- **THEN** normalization and artifact lookup preserve every value without requiring the legacy bitmask helper

#### Scenario: Legacy helper cannot represent the selection
- **WHEN** a caller explicitly uses ShaderVariantKey with an unsupported width
- **THEN** it reports failure instead of truncating the logical selection

### Requirement: 全局 pipeline 位预留

`GlobalVariantLayout` SHALL 定义引擎级 pipeline 位预留区（固定 bit 位置），所有 shader schema 的 per-shader 位 SHALL 排在预留区之后；Pipeline 的变体组装 SHALL 只用 `GlobalVariantLayout` 设 pipeline 位，不依赖具体 shader schema。

#### Scenario: pipeline 位跨 shader 一致

- **WHEN** 两个不同 shader 的 schema 都引用同一 `GlobalVariantLayout`
- **THEN** `"shadows"` 在两个 shader 里映射到相同 bit 位

### Requirement: 机制由 shader 声明决定

Business inputs SHALL NOT select macro/native policy or native IDs. JSON companions SHALL retain logical key metadata and any explicit source-symbol association. The compiler SHALL derive target implementation and native mappings from validated shader declarations and target support. Supported macro or equivalent compile-time paths SHALL consume the selected values; native paths SHALL retain their stage/type/ID mapping. Unsupported conversion SHALL fail rather than becoming a uniform or using a default.

#### Scenario: One key across targets
- **WHEN** a shader key uses native specialization on a supported target and an explicitly supported compile-time path on another
- **THEN** the caller supplies the same semantic key/value and both execute that value

#### Scenario: Unrelated source symbol
- **WHEN** a source-symbol mapping is missing, ambiguous or semantically incompatible
- **THEN** compilation reports the key/stage/target rather than guessing from numeric IDs

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
