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

native pipeline layout SHALL 由各后端在 `Shader` 内从 shader 反射派生，而非由调用方经独立对象传入：

- Vulkan：`VulkanShader` 自建 `VkPipelineLayout`（`CreatePipelineLayout()`，由 `Shader::Descriptor.reflection` 构建 descriptor set layout + push constants），pipeline state 统一用 `VulkanShader::GetPipelineLayout()`
- DX12：root signature 由 `D3D12Shader` 从反射构建（`D3D12RootSignature`），pipeline state 统一用 `d3dShader->GetRootSignature()`
- Metal：argument buffer 由 `MetalShader` 从反射构建

#### Scenario: Vulkan pipeline 使用 shader 布局

- **WHEN** 创建 `VulkanGraphicsPipeline`/`VulkanComputePipeline`（`Descriptor` 无 layout）
- **THEN** `VkGraphicsPipelineCreateInfo::layout` / `VkComputePipelineCreateInfo::layout` 取自 `vkShader->GetPipelineLayout()`，该 layout 由反射构建

### Requirement: push constants 归入 shader 反射

`PipelineLayout::Descriptor::pushConstants` SHALL 被删除；push-constant 区间（offset/size/stages）SHALL 由 shader 声明并经反射得到，与 descriptor set/binding 同源。

#### Scenario: 无 PipelineLayout push constants

- **WHEN** 尝试在 layout 描述符里声明 push constants
- **THEN** 无此字段（编译失败）；push constants 改由 shader 反射承载

### Requirement: shader 派生的 set layout 供 ResourceGroup 复用

shader 内派生的 per-set descriptor set layout SHALL 不仅用于 pipeline layout，也作为 ResourceGroup 分配 descriptor 的唯一来源。

- Vulkan：`VulkanShader` SHALL 暴露 per-set 的 `VkDescriptorSetLayout`（按真实 set 索引查询），`VulkanResourceGroup` 用它 `vkAllocateDescriptorSets`，保证 `VkDescriptorSet` 与 `VkPipelineLayout` 同源。
- DX12：`D3D12Shader` SHALL 暴露 reflection，`D3D12ResourceGroup` 从 reflection 计算 set 的 descriptor 数量。
- Metal：`MetalShader` SHALL 从反射派生 argument buffer 布局供 ResourceGroup 复用（后续实现）。

#### Scenario: Vulkan set layout 按真实 set 索引查询

- **WHEN** shader reflection 含 set 0 与 set 2（set 索引有空洞）
- **THEN** `GetDescriptorSetLayout(0)` 与 `GetDescriptorSetLayout(2)` 返回各自正确的 per-set layout，而非按数组下标错位

#### Scenario: ResourceGroup 复用 shader set layout

- **WHEN** 用同一 shader 分别创建 pipeline 与 ResourceGroup{shader, set}
- **THEN** ResourceGroup 的 descriptor set 与 pipeline layout 的对应 set 使用同一 `VkDescriptorSetLayout`

### Requirement: shader 反射进入 RHI Shader

后端无关的 `ShaderReflection` SHALL 定义在 `aurora/rhi` 接口层（`aurora/rhi/ShaderReflection.h`），`Shader::Descriptor` SHALL 携带 `const ShaderReflection *reflection`。`aurora/shader` 编译器 SHALL 填充该反射，各后端 SHALL 消费它构建 native layout。

#### Scenario: 反射可被后端消费

- **WHEN** `aurora/shader` 编译产出 `ShaderReflection`（set/binding/type/stage），并把它填入 `Shader::Descriptor.reflection`
- **THEN** 后端 `Shader::Init` 能读到该反射，无需反向依赖 `aurora/shader`

### Requirement: Vulkan layout 由反射派生

`VulkanShader::CreatePipelineLayout()` SHALL 从 `Shader::Descriptor.reflection` 构建 `VkDescriptorSetLayout`（按 set 分组，binding/descriptorType/count/stageFlags 来自反射）与 `VkPipelineLayout`（含 push constants），不再创建空 layout。

#### Scenario: 反射驱动 descriptor set layout

- **WHEN** `Shader::Descriptor.reflection` 含 set 0 的 UBO binding 0、set 1 的 combined image sampler
- **THEN** `VulkanShader` 产出含两个 `VkDescriptorSetLayout` 的 `VkPipelineLayout`，binding 类型/可见 stage 与反射一致

### Requirement: DX12 root signature 由反射派生

`D3D12Shader::Init` SHALL 从反射构建 `D3D12RootSignature`（set → descriptor table，range 来自 binding/type/stage）。`D3D12GraphicsPipeline`/`D3D12ComputePipeline` SHALL 统一使用 `d3dShader->GetRootSignature()`；`D3D12ComputePipeline::Init` SHALL 不再接收外部 root signature 参数。

#### Scenario: PSO 使用 shader 的 root signature

- **WHEN** 创建 `D3D12GraphicsPipeline`/`D3D12ComputePipeline`
- **THEN** `pRootSignature` 取自 `d3dShader->GetRootSignature()`（由反射构建），无外部传入

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

`Shader::Descriptor::reflection` 是 `const ShaderReflection*`，MUST 非 null：shader 创建与 `CreateResourceGroup` 遇 null reflection SHALL 拒绝并 logger 报错。非 null 但 `resources` / `pushConstants` / `blocks` 均为空的"空 reflection" SHALL 合法，对应无资源 shader 的空 set 布局。

#### Scenario: null reflection 拒绝

- **WHEN** `Shader::Descriptor::reflection = nullptr` 创建 shader 或 `CreateResourceGroup({shader, set})` 且 shader 无 reflection
- **THEN** 返回 nullptr 并 logger 报错

#### Scenario: 空 reflection 合法

- **WHEN** shader 的 reflection 非 null 但 `resources` 为空，调用 `CreateShader`
- **THEN** shader 创建成功（空 reflection 不报错）；随后 `CreateResourceGroup({shader, set})` 对该 shader 中不存在的 set 返回 nullptr

### Requirement: ResourceGroup 复用 shader 派生的 native set layout

各后端 ResourceGroup SHALL 复用 shader 内已派生的 native layout，而非自建：

- Vulkan：`VulkanShader` SHALL 暴露 `VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set)`，`VulkanResourceGroup` 用它 `vkAllocateDescriptorSets`。
- DX12：`D3D12ResourceGroup` SHALL 从 shader reflection 计算 set 的 descriptor 数量并分配 heap 区间。
- Metal：`MetalResourceGroup` SHALL 从 shader 派生的 argument buffer 布局分配（后续实现）。

#### Scenario: Vulkan descriptor set 与 pipeline layout 同源

- **WHEN** 用同一 shader 创建 pipeline 与 ResourceGroup
- **THEN** ResourceGroup 的 `VkDescriptorSet` 与 pipeline 的 `VkPipelineLayout` 使用同一 `VkDescriptorSetLayout`（非 compatible 的两套）

### Requirement: ResourceGroup 布局等于 shader reflection 的完整 set

ResourceGroup 的 binding 集合 SHALL 等于 shader reflection 中该 set 的全部资源；SHALL NOT 存在"手写少于 shader 的 layout"。未 `Update` 的 binding 保持"未写入"（Vulkan 对应 null descriptor）。

#### Scenario: 布局即 shader 完整 set

- **WHEN** shader reflection 的 set 0 含 binding 0（UB）+ binding 1（sampled image），创建 ResourceGroup{shader, 0}
- **THEN** ResourceGroup 覆盖这两个 binding；只 Update binding 0 时 binding 1 保持未写入

### Requirement: shader 生命周期不因 ResourceGroup 延长

shader SHALL 是派生 native layout 的短命对象，创建完 pipeline 后即可释放，不因 ResourceGroup 而延长生命周期，也 SHALL NOT 需要 delayed release。ResourceGroup 存续期间其 native set layout MUST 存活（最小落地：ResourceGroup 持 shader 引用；native layout 独立所有权为后续优化）。

#### Scenario: shader 不因 ResourceGroup 而无限延长

- **WHEN** 创建 pipeline 与 ResourceGroup 后释放调用方 shader 引用
- **THEN** shader 生命周期由 pipeline 与 ResourceGroup 的引用共同决定，且无 delayed release 语义；ResourceGroup 存续期间 set layout 不悬空

### Requirement: UBO 类型按显式绑定声明

`ShaderResourceType` SHALL 含 `UNIFORM_BUFFER_DYNAMIC` 与 `STORAGE_BUFFER_DYNAMIC` 变体。Global/Pass/Batch 的静态或动态绑定 SHALL 由具体 Pass/材质契约显式选择，不得仅由 set 编号推导。

DYNAMIC 变体非 shader 反射自然产物。共享声明 SHALL 描述基础 uniform 类型；调用方 SHALL 使实际 shader/layout 的动态类型、分配范围和绑定偏移保持一致。

#### Scenario: 显式动态绑定

- **WHEN** 调用方将 batch resource 的类型显式选择为 `UNIFORM_BUFFER_DYNAMIC`
- **THEN** ResourceGroup{shader, 2} 派生出的 layout 为 dynamic UBO，`BindResourceGroup(2, rg, 1, &offset)` 生效

#### Scenario: 静态绑定

- **WHEN** global（set 0）/ pass（set 1）block 的 reflection resource 类型为 `UNIFORM_BUFFER`
- **THEN** ResourceGroup{shader, 0} 与 {shader, 1} 的 UBO 为 static，无 dynamic offset

### Requirement: Pass-owned shared header source
Each Global/Pass/Batch declaration SHALL have one editable shared `.h` source consumed directly by C++ and Slang. Field/resource lists SHALL define native C++ types, typed binding slots, expected schema and shader declarations without duplicating fields or binding numbers. Global/Pass/Batch SHALL remain sets 0/1/2; View SHALL remain optional and owned by the consuming Pass.

#### Scenario: Build C++ without shader code generation
- **WHEN** Pipeline C++ consumers compile
- **THEN** they include source headers directly and have no ShaderHeaders, ShaderHeaderTool or Slang execution prerequisite; CMake may embed source text for runtime shader includes

#### Scenario: Shared source changes
- **WHEN** a shared declaration or common shader helper changes
- **THEN** C++ consumers rebuild and CMake refreshes the embedded source; no mirrored ABI header in the source or build tree is consumed

#### Scenario: Uniform layout across targets
- **WHEN** a shared flat uniform block is compiled for SPIR-V, DXIL or MSL
- **THEN** it uses explicit logical set/binding ConstantBuffer declarations and column-major matrices; expected C++ sizeof/offsetof and reflected target layout must agree

### Requirement: 虚拟 include
`ShaderCompilerSlang` SHALL resolve quoted and angle includes through `ShaderFileSystem`, including packaged shared headers and their common helper, without requiring disk copies or native host header generators.

#### Scenario: Packaged scene shaders
- **WHEN** only the built-in scene virtual files are registered, without filesystem search paths
- **THEN** SceneForward, SceneLit, SceneShadow and SceneToneMap compile for each supported target with their shared declaration includes

#### Scenario: Shared helper cache invalidation
- **WHEN** a shared helper reached through an angle include changes
- **THEN** the source closure hash changes and old cache entries are rejected; quoted includes search the including directory before the logical root

### Requirement: 类型映射
Shared declaration helpers SHALL map float/int/uint, float2/float4 and column-major mat4 fields to native C++ types and Slang fields, and preserve bounded float4 array counts/strides. BOOL/FLOAT3 uniform fields (use UINT/FLOAT4), nested structures, storage-buffer declarations, descriptor arrays and unsupported uniform arrays SHALL fail explicitly instead of omitting fields. Resource-only declarations SHALL preserve resource shape and typed slots; describing a resource SHALL NOT imply backend binding support.

#### Scenario: Bounded uniform arrays
- **WHEN** a Pass declares a float4 array
- **THEN** native sizeof/offsetof, array count and stride agree with each target's reflection

#### Scenario: Resource-only declaration
- **WHEN** a shared declaration contains textures or samplers without constants
- **THEN** it exposes resource schema and typed slots without inventing a placeholder uniform block

### Requirement: 三方一致性校验
Shared C++ types and their expected schema SHALL use native sizeof/offsetof. Target shader reflection SHALL validate resource shapes and actual uniform members, offsets, sizes and strides before binding; the legacy RgBlockDesc bridge SHALL remain derived from the same field list. Tests SHALL verify shared declarations against SPIR-V, DXIL and MSL independently. MSL source/reflection tests SHALL NOT replace native Metal execution validation.

#### Scenario: Layout drift is rejected
- **WHEN** compiled shader layout disagrees with the shared C++ schema
- **THEN** binding returns an explicit error before modifying the draw or executing it on the GPU

#### Scenario: Cross compilation
- **WHEN** a target platform C++ build consumes a Pass header
- **THEN** no host shader generator is required to obtain its types; target layout and execution still require corresponding shader/platform validation

### Requirement: 统一变体模型

aurora.shader SHALL 提供统一 `Name→value` 变体模型，接口与 cache key 不区分强/弱：

- `ShaderVariantEntry{Name key, uint32 value}` — key 与 shader 里宏名/spec 常量名一对一
- `ShaderVariant` — 一次具体变体选择（entries 列表）

#### Scenario: 统一表达

- **WHEN** 构造 `ShaderVariant`，含 `{"USE_SHADOWS", 1}` 与 `{"NUM_LIGHTS", 4}`
- **THEN** 两者是同一类型条目，无强/弱区分

### Requirement: bitmask key 与数据驱动 schema

`ShaderVariantKey` SHALL 为 128bit bitmask（`uint64_t words[2]` + `totalBits`），`totalBits > 128` 时 schema 构建 SHALL 报错。`ShaderVariantSchema` SHALL 数据驱动：source 为 `Name` tag（非 enum），位布局/宽度由 schema 定义。

#### Scenario: 越界报错

- **WHEN** schema 的 `totalBits > 128`
- **THEN** 构建失败（报错），不静默截断

#### Scenario: 多来源 OR 组合

- **WHEN** 不同 source 拥有不相交位区间，各自 `Set` 后 `operator|=`
- **THEN** 组合结果等于无冲突拼接

### Requirement: 全局 pipeline 位预留

`GlobalVariantLayout` SHALL 定义引擎级 pipeline 位预留区（固定 bit 位置），所有 shader schema 的 per-shader 位 SHALL 排在预留区之后；PipelinePass SHALL 只用 `GlobalVariantLayout` 设 pipeline 位，不依赖具体 shader schema。

#### Scenario: pipeline 位跨 shader 一致

- **WHEN** 两个不同 shader 的 schema 都引用同一 `GlobalVariantLayout`
- **THEN** `"shadows"` 在两个 shader 里映射到相同 bit 位

### Requirement: 机制由 shader 声明决定

编译层 SHALL 对每个 key 反射解析：命中 `[SpecializationConstant]` 同名声明 → spec 常量；否则 → `-D name=value` 宏。SPIRV/MSL spec 常量 SHALL 走 native specialize（同二进制），DXIL SHALL 折叠。

#### Scenario: spec vs 宏解析

- **WHEN** key `"NUM_LIGHTS"` 在 shader 里声明为 `[SpecializationConstant(0)]`，key `"USE_SHADOWS"` 未声明
- **THEN** 前者走 spec 常量（SPIRV/MSL native、DXIL 折叠），后者走 `-D USE_SHADOWS=...`

### Requirement: 反射是编译产物

反射 SHALL 随编译结果（`{二进制, 反射}`）一起缓存；强变体（宏）SHALL 可改 layout，弱变体（spec 常量）SHALL 不改。反射不构成 cache key 维度。

#### Scenario: 强变体改 layout

- **WHEN** 宏 `#if` 增删一个资源绑定
- **THEN** 不同宏值编译出的反射 layout 可不同（作为编译产物的一部分）

### Requirement: 统一 cache key 与 ShaderCache 接口

`ShaderCacheKey` SHALL 统一（variantHash 不分强/弱）并扩展为 `{sourceHash, variantHash, target, stage, entryHash, layoutFp, schemaFp, toolchainFp}`。`ShaderCache`（Load/Store）接口 SHALL 由 `shader-cache` change 落地为内容寻址存储后端（offline 只读 / local 可写两 root）；`ShaderCompileDesc` 的 `cache` 字段 SHALL 被使用。

#### Scenario: 统一 key

- **WHEN** 同一 source + 同一 variant + 同一 target + 同一 layout/schema/toolchain 编译两次
- **THEN** 产生相同 `ShaderCacheKey`；`cache == nullptr` 时走直接编译

#### Scenario: key 含工具链与布局

- **WHEN** source / variant / target 相同，但 `toolchainFp` 或 `layoutFp` 不同
- **THEN** `ShaderCacheKey` 不同，不会命中旧产物

### Requirement: Slang 编译通道

`ShaderCompilerSlang` SHALL 经 Slang Compilation API（`IGlobalSession` / `ISession` / `loadModuleFromSourceString` / compose+link / `getEntryPointCode`）把 Slang 源码编译为 SPIRV 与 MSL 双产物。

Slang 程序布局（program layout）SHALL 收敛到统一 `ShaderReflection`（`ShaderResource{set, binding, type, name}`）；`space` 映射 set。

`ShaderCompilerSlang` SHALL 支持虚拟 include：经 `ShaderFileSystem` 解析 shader 中的 `#include`，命中内存中的共享声明源，无需落盘。

#### Scenario: SPIRV 产物

- **WHEN** 编译含 ParameterBlock 的 mini shader 目标 SPIRV
- **THEN** 产物非空；Slang program layout 反射出各 block 的 set/binding 与源码声明一致

#### Scenario: MSL 产物直出

- **WHEN** 同一 shader 目标 MSL
- **THEN** 产物为 MSL 文本，不经 SPIRV-Cross；ParameterBlock 翻译为 argument buffer 形态（`constant* ... [[buffer(N)]]`）

#### Scenario: 反射收敛

- **WHEN** Slang 编译完成
- **THEN** `ShaderBuildResult.reflection.resources` 含各资源的 set/binding/type/name

#### Scenario: 虚拟 include

- **WHEN** shader 源码含 `#include "Generated/GlobalBlock.slang"`
- **THEN** 编译器经 `ShaderFileSystem` 解析该路径为内存内容并成功编译；该路径不存在于磁盘

