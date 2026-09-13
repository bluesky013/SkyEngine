## Context

`PipelineLayout` 接口已移除（`cbf0e7a5`），native binding layout 统一由 shader reflection 派生：`VulkanShader::CreatePipelineLayout()` 已从 reflection 创建 per-set 的 `VkDescriptorSetLayout`（`VulkanShader.cpp:97`），DX12 从 reflection 派生 root signature，Metal 预留 argument buffer。但 `ResourceGroup` 创建仍依赖独立的 `ResourceGroupLayout` 对象（`ResourceGroup::Descriptor::layout`），该对象承载的 binding 信息与 shader reflection 完全重复，且 Vulkan 端形成两套 `VkDescriptorSetLayout`。二者目前靠 codegen 的 `RgBlockDesc` 单一事实源"碰巧"一致。

本 change 是 `cbf0e7a5` 的下一半：把 `ResourceGroupLayout` 也收掉，resource binding 完全以 shader reflection 为唯一来源。

## Goals / Non-Goals

**Goals:**

- 删除 `ResourceGroupLayout` 接口对象与 `Device::CreateResourceGroupLayout`。
- `ResourceGroup` 创建改为 `{shader, set}`，各后端复用 shader 内已派生的 native layout。
- 清理 `DescriptorType` / `DescriptorBindingFlags` 枚举，binding 类型统一用 `ShaderResourceType`。
- 界定 shader reflection 的 null / empty 语义。
- 上层 `aurora/pipeline` 删除 `RgBlockDesc → ToLayoutDescriptor` 链路，global / pass 用 dummy shader 派生 set 布局。
- 保持 `ResourceGroup::Update` / `BindResourceGroup` / `PushConstants` 语义不变（调用方无感）。

**Non-Goals:**

- 不实现 Metal 后端 ResourceGroup（本 change 只定接口）。
- 不实现 tier2 descriptor heap（`DescriptorHeap` 接口不变）。
- 不解决 DYNAMIC（batch tier dynamic UBO）在移除 `DescriptorType` 后的表达（见 Open Questions）。
- 不改变 `RgBlockDesc` codegen 职责。

## Decisions

### 决策 1：`ResourceGroup::Descriptor` 改为 `{shader, set}`

```cpp
class ResourceGroup : public RefObject, public IDelayReleaseResource {
public:
    struct Descriptor {
        Shader  *shader = nullptr;
        uint32_t set    = 0;
    };
    void Update(const std::vector<ResourceUpdateInfo> &writes);
};
```

- `shader` MUST 非空；后端 `CreateResourceGroup` 校验并 `static_cast` 到具体 Shader。
- 备选 `{ShaderReflection*, set}` 否决：Vulkan 需要 native `VkDescriptorSetLayout`（由 `VulkanShader` 创建），纯 reflection 数据不持有它。

### 决策 2：reflection 的 null 与 empty 语义

`Shader::Descriptor::reflection` 是 `const ShaderReflection*`（`Shader.h:54`）。界定：

- **null**（指针为 null）：错误。shader 创建（`CreateShader`）与 `CreateResourceGroup` 均 MUST 拒绝并 logger 报错——reflection 是编译 shader 的必然产物，缺它是调用方 bug。
- **empty**（非 null 但 `resources` / `pushConstants` / `blocks` 均为空）：合法。无资源 shader（如纯 push constant 或 trivial shader）的 set 布局为空。

### 决策 3：ResourceGroup 布局 = shader reflection 中该 set 的完整 binding 集

移除后无法再手写一个少于 shader 的 layout。ResourceGroup 的 set 布局即 shader reflection 里该 set 的全部资源。未 `Update` 的 binding 保持"未写入"（Vulkan null descriptor，DX12 未填充 heap 槽）。

### 决策 4：Vulkan 复用 shader 派生的 `VkDescriptorSetLayout`

- `VulkanShader` 暴露 `VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const`。
- `VulkanResourceGroup::Init` 用该 layout `vkAllocateDescriptorSets`。
- **修复 set 索引映射**：当前 `descriptorSetLayouts` 是 `std::vector`，按 `std::map`（set 升序）遍历 push_back，未记录 element→set 映射；有 set 空洞（如只用 set 0 + set 2）时 vector index ≠ set index。改为保存 set 索引（`std::vector<uint32_t> setIndices` 或 `std::map<uint32_t, VkDescriptorSetLayout>`）。

### 决策 5：shader 是短命对象，layout 所有权独立于 shader

- shader 是派生 native layout 的输入，创建完 pipeline 后即可释放，**不因 ResourceGroup 而延长生命周期**，也**不需要 delayed release**（`IDelayReleaseResource`）——shader 是 CPU 侧 layout 工厂，不是 GPU 资源。
- pipeline 持有 pipeline layout 的 native 句柄；ResourceGroup 持有 set layout 的 native 句柄。
- **技术后果**：当前 `VulkanShader` 析构会 `vkDestroyPipelineLayout` + `vkDestroyDescriptorSetLayout`（`VulkanShader.cpp:89-94`）。要支持 shader 短命，native layout 的所有权须独立于 shader。本 change 内最小落地：`VulkanResourceGroup` 持 `CounterPtr<VulkanShader>` 保证 set layout 存活（shader 生命周期覆盖 RG，但无 delayed release 语义）；"native layout 完全独立所有权 / device 级 layout cache"作为后续优化（与 pipeline 不再持 shader 一起处理）。

### 决策 6：DX12 从 reflection 计算 descriptor 数量

- 删除 `D3D12ResourceGroupLayout`。
- `D3D12Shader` 暴露 `const ShaderReflection &GetReflection()`。
- `D3D12ResourceGroup::Init` 从 reflection 里该 set 的资源列表计算 cbvSrvUav / sampler 数量与 per-binding offset（等价于现在 `D3D12ResourceGroupLayout::Init` 的计数逻辑，输入从手写 bindings 换成 reflection resources），然后分配 heap 区间。

### 决策 7：清理 `DescriptorType` / `DescriptorBindingFlags`，DYNAMIC 迁入 `ShaderResourceType`

移除 `ResourceGroupLayout` 后，`DescriptorType` 与 `DescriptorBindingFlags`（`Core.h:240` / `363`）失去唯一使用方，整体删除；binding 类型统一用 `ShaderResourceType`（`ShaderReflection.h:17`）。

**DYNAMIC 保留**：`ShaderResourceType` 增加 `UNIFORM_BUFFER_DYNAMIC` 与 `STORAGE_BUFFER_DYNAMIC` 变体（对齐原 `DescriptorType`），因为 batch tier（set 2）需要 dynamic UBO。该变体并非 SPIR-V 反射自然产物（SPIR-V 层面 uniform buffer 不分 static/dynamic），而是由上层 codegen 按 `RgBlockDesc.kind` 在构建 reflection 时标记（见决策 9）。

**combined 删除**：reflection 无 combined（separate only），删除 `COMBINED_IMAGE_SAMPLER` 枚举值及其 write 分支。

- `D3D12RootSignature::RootSignatureDescriptorRange.type` 改 `ShaderResourceType`，`ToRangeType` 改收 `ShaderResourceType`。
- `D3D12ShaderFunction.cpp` 的 `FromShaderResourceType`（返回 `DescriptorType`）删除，root signature 直接消费 `ShaderResourceType`。
- `VulkanConversion::FromDescriptorType` / `D3D12Conversion::FromDescriptorType` 删除；`VulkanShader::FromShaderResourceType` 扩以映射 DYNAMIC 变体。

### 决策 8：上层 global / pass 用 dummy shader 派生 set 布局

global（set 0）/ pass（set 1）的 block 声明由 codegen 生成（`GlobalBlock.slang` → `GlobalBlock.gen.h`）。为创建对应 tier 的 ResourceGroup，用一个 **dummy shader**（仅含对应 block 声明的最小 shader，编译得到 reflection + native set layout）派生 set 布局：

- `GlobalRenderResources::Init` 改为接收（或内部编译）global dummy shader，`CreateResourceGroup({dummyShader, 0})`。
- `PipelinePass::RebuildPassResources` 同理，用 pass dummy shader + set 1。
- dummy shader 只为提供 set layout，不参与实际绘制；可离线 codegen 生成源码、运行时编译。

### 决策 9：UBO 类型按 tier 约定（global/pass static，batch dynamic）

- set 0（global）与 set 1（pass）的 UBO SHALL 为 static `UNIFORM_BUFFER`。
- set 2（batch）的 UBO SHALL 为 dynamic `UNIFORM_BUFFER_DYNAMIC`（配合 `BatchAllocator` 的 per-draw offset，`BindResourceGroup(2, rg, 1, &offset)`）。

DYNAMIC 的标记来源是 `RgBlockDesc.kind`（`CBUFFER` vs `CBUFFER_DYNAMIC`，`RgBlockDesc.h:43-47`）：codegen 生成 reflection 时，batch block 的 `kind = CBUFFER_DYNAMIC` 使其在 reflection 中标记为 `UNIFORM_BUFFER_DYNAMIC`。具体标记机制（slang attribute 或 reflection 后处理）在 batch tier 实现 change 细化。

### 决策 10：dynamic offset 的后端实现（Vulkan 原生 / D3D12 root CBV / Metal setBuffer offset）

三者 static binding 统一走 descriptor（Vk descriptor set / D3D12 descriptor table / Metal argument buffer），dynamic binding 走 per-draw offset，但后端机制不同：

- **Vulkan**：原生支持。`VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC` + `vkCmdBindDescriptorSets` 的 `pDynamicOffsets`。无需额外工作。

- **D3D12**：D3D12 无 descriptor-level dynamic offset，用 **root CBV**（`D3D12_ROOT_PARAMETER_TYPE_CBV`，直接持有 `D3D12_GPU_VIRTUAL_ADDRESS`）：
  - root signature 里，DYNAMIC uniform/storage buffer 的 binding 拆成独立 root param（root CBV），`Descriptor.ShaderRegister = binding`、`Descriptor.RegisterSpace = set`，**不进 descriptor table**。
  - `BindResourceGroup` 对 dynamic binding 调 `SetGraphicsRootConstantBufferView(rootParam, bufferGPUAddress + dynamicOffset)`。
  - dynamic UBO 不占 descriptor heap；`D3D12ResourceGroup::Update` 对 dynamic binding 不写 descriptor，仅记录 buffer 供 bind 时算地址。
  - root signature 布局从"每 set 一个 static descriptor table"扩展为"每 set 一个 static table + N 个 dynamic root CBV"；`D3D12RootSignature` 保存 dynamic binding → root param 映射，`D3D12Encoder::BindResourceGroup` 按 set 查 root param 传 offset。
  - 约束：root CBV 占 root signature 空间（每个 2 DWORDs，root signature 上限 64 DWORDs），dynamic binding 数量受此约束；超出需退化为 descriptor table + 重写 descriptor（本 change 不做）。

- **Metal**：用 `setBuffer offset:` 的 offset 参数：
  - batch 的 dynamic UBO 是 `BatchAllocator` 的大 `MTLBuffer`，每 draw 一个 256B 对齐 offset。
  - `BindResourceGroup(2, rg, 1, &offset)` 时，Metal encoder 对 dynamic binding 调 `[enc setVertexBuffer:buffer offset:offset atIndex:slot]`（fragment 用 `setFragmentBuffer`，compute 用 `setBuffer`）。
  - dynamic UBO 不放进 argument buffer descriptor，而是每 draw 直接 setBuffer offset 到对应 MSL buffer slot。
  - slot 映射来自 MSL 反射（SPIRV-Cross 的 `(set,binding) -> [[buffer(N)]]` 重映射表）。

## Risks / Trade-offs

- **set 索引映射修复引入 regression 风险** → 决策 4 修 `descriptorSetLayouts` 时补 `GetDescriptorSetLayout(set)` 单元测试（含空洞 set 用例）。
- **native layout 所有权未完全独立**（决策 5 最小落地持 shader 引用）→ 记录为后续优化；shader 仍被 RG 持引用，严格意义上"建完 PSO 即弃"要到 layout 独立所有权那步才完全达成。
- **清理 `DescriptorType` 波及面广**（12+ 处）→ 用 `ShaderResourceType` 一处替换，编译期暴露遗漏；`RootSignatureDescriptorRange.type` 类型变化是编译期错误，不会被静默。
- **DX12 计数逻辑换输入源** → 复用 `FromShaderResourceType` 语义，COMBINED 在 reflection 已是 separate（SAMPLED_IMAGE + SAMPLER），计数等价。
- **DYNAMIC / combined 在 reflection 无类型**（决策 7 副作用）→ 见 Open Questions；本 change 删除其接口层表达，不静默降级。

## Migration Plan

1. 接口层：改 `ResourceGroup.h`（删 `ResourceGroupLayout`、改 `Descriptor`、删 `COMBINED_IMAGE_SAMPLER` write kind）、`Core.h`（删 `DescriptorType` / `DescriptorBindingFlags`）、`Device.h`（删 `CreateResourceGroupLayout`）、`Shader.h`（reflection null 语义注释）。
2. Vulkan：`VulkanShader` 暴露 `GetDescriptorSetLayout` + 修 set 映射 + reflection null 校验；`VulkanResourceGroup` 复用；删 `VulkanResourceGroupLayout`；`VulkanConversion` 删 `FromDescriptorType`。
3. DX12：`D3D12Shader` 暴露 reflection；`D3D12ResourceGroup` 从 reflection 计算；`D3D12RootSignature` 类型改 `ShaderResourceType`；删 `D3D12ResourceGroupLayout`；`D3D12Conversion` 删 `FromDescriptorType`。
4. Metal：`MetalDevice` 删 `CreateResourceGroupLayout` stub。
5. 上层：`RgBlockDesc.cpp` 删 `ToLayoutDescriptor`；`GlobalRenderResources` / `PipelinePass` 改用 dummy shader + `{shader, set}`。
6. 测试：`ResourceGroupTest` 改"编译 shader → `{shader, set}` 建 RG"；`RgBlockDescTest` 删 layout 转换用例；补空洞 set 映射用例。
7. 构建 + 全量回归（Vulkan + DX12 编译通过，Vulkan 测试无回归）。

## Open Questions

- **DYNAMIC 标记机制**（已定方向，机制未定）：决策 9 确定 batch 用 dynamic UBO、`ShaderResourceType` 加 DYNAMIC 变体；但"codegen 如何把 `CBUFFER_DYNAMIC` 标进 reflection"（slang attribute 还是 reflection 后处理）留待 batch tier 实现 change 细化。
- **combined 的最终确认**：决策 7 删 `COMBINED_IMAGE_SAMPLER`，即 Aurora 强制 separate（`Texture2D` + `SamplerState` 分开声明）。确认这是既定方向（`DescriptorHeap.h:16` 注释已暗示 untyped path 不支持 combined）。
- **native layout 完全独立所有权 / device 级 cache**：决策 5 最小落地持 shader 引用；是否要更早引入独立 layout RefObject 以彻底达成"shader 建完 PSO 即弃"。
