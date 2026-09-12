## Context

现状（`dev_refactor_rhi`，`aurora-shader-derived-layout` 已落地）：

- RHI `Shader`/`ShaderFunction` 只携带二进制（`ShaderBinaryProvider::binaryData`）与 stage，无反射。
- `aurora/shader` 的 `ShaderReflection`（`ShaderResource{set,binding,type,name}` + `ShaderBlockLayout{set,binding,size,members}`）是后端无关的布局事实源，但只在 shader 模块内可见（`aurora/shader → aurora/rhi` 依赖方向，rhi 不能反向 include）。
- Vulkan：`VulkanShader::CreatePipelineLayout()` 建空 `VkPipelineLayout`。
- DX12：`D3D12RootSignature` 已有 range/set 生成逻辑，但 `D3D12Shader::Init` 不建 `rootSignature`（`GetRootSignature()` 返回空），`D3D12ComputePipeline::Init` 仍收外部 `rootSig`。
- Metal：`MetalShader` 未建 argument buffer。

约束：`aurora/shader → aurora/rhi` 单向依赖；`ResourceGroup`/descriptor 绑定更新属 `aurora-resource-group`（本 change 不碰）。

## Goals / Non-Goals

**Goals:**

- 让 shader 反射流进 RHI `Shader`，后端据此自建 native layout。
- Vulkan：`VkDescriptorSetLayout` + `VkPushConstantRange` + `VkPipelineLayout` 由反射填充。
- DX12：`D3D12RootSignature` 由反射填充，PSO 统一用 `d3dShader->GetRootSignature()`，删除外部 rootSig 参数。
- Metal：`MetalShader` 由反射建 argument buffer（尽力/stub 一致）。

**Non-Goals:**

- 不实现 descriptor 绑定/更新（`aurora-resource-group`）。
- 不实现完整 DX12 descriptor table 分配（root signature 只建 range 结构，heap 分配后续）。
- 不引入第二套反射实现（复用 `aurora/shader` 的 `ShaderReflection`）。

## Decisions

### 1. 反射结构下沉到 `aurora/rhi` 接口层

把 `ShaderReflection`（及 `ShaderResource`/`ShaderBlockLayout`/`ShaderResourceType` 等）从 `aurora/shader` 迁到 `aurora/rhi`（`aurora/rhi/ShaderReflection.h`）。理由：rhi 是 `aurora/shader` 的下层依赖，rhi 可被 shader 编译器与各后端共同 include；反射结构本身后端无关（仅 std 类型 + rhi 枚举）。

**备选**（rhi 定义新反射结构，shader 侧转换）被否：重复定义，增加转换层。

### 2. 反射挂在 `Shader` 层

反射描述整个 program 的 layout（跨 stage 的 set/binding），放在 `Shader`（而非 `ShaderFunction`）。但当前 spike 是 per-function 独立编译（VS/FS 各自 `Compile` 产生各自 `ShaderCompileResult.reflection`）。

决策：`Shader::Descriptor` 增加 `const ShaderReflection *reflection = nullptr`（整体 layout）；`ShaderFunction::Descriptor` 保持二进制+stage。spike 的 per-function 反射由调用方合并后传入 `Shader`（当前合并逻辑留 Open Question）。Vulkan/DX12 的 `Shader::Init` 读取 `desc.reflection` 建 layout。

### 3. Vulkan：反射 → descriptor set layout + pipeline layout

`VulkanShader::CreatePipelineLayout()` 从 `desc.reflection` 构建：
- 按 `set` 分组，每个 set 生成 `VkDescriptorSetLayoutBinding[]`（binding/descriptorType/count/stageFlags）→ `VkDescriptorSetLayout`。
- push constants（若反射含）→ `VkPushConstantRange[]`。
- `VkPipelineLayoutCreateInfo{setLayouts, pushConstantRanges}` → `VkPipelineLayout`。

`ShaderReflection` 需能表达 push constants（当前无 → 补 `PushConstantRange` 字段或沿用 `rhi/Core.h` 的 `PushConstantRange`）。

### 4. DX12：反射 → root signature

`D3D12Shader::Init` 由反射填 `RootSignatureDescriptor`（set → descriptor table，range 来自 binding/type/stage），调用现有 `D3D12RootSignature::Init`。`D3D12ComputePipeline::Init(const Descriptor&)` 去掉外部 `rootSig` 参数，统一 `d3dShader->GetRootSignature()`。

### 5. Metal：反射 → argument buffer

`MetalShader` 由反射建 argument buffer（MSL 的 `[[buffer(N)]]` 绑定）。macOS only，本 change 做接口对齐 + stub，实际填充可后续。

## Risks / Trade-offs

- **[反射结构与 rhi 依赖]**：`ShaderReflection` 迁移到 rhi 后，`aurora/shader` 的 include 需改 `<aurora/rhi/ShaderReflection.h>`。影响 `ShaderCompilerSlang`/`ShaderCodeGen`/测试，属机械改动。
- **[descriptor set layout 需完整反射]**：Vulkan 建 `VkDescriptorSetLayout` 依赖每个 binding 的 type/count/stage。`ShaderReflection` 已有这些（`ShaderResource` + `ShaderBlockLayout`），但需确认 texture/sampler 类型映射齐全（`FromSlangCategory` 已有）。
- **[push constants 反射缺失]**：当前 `ShaderReflection` 无 push constants。需补（Vulkan `VkPushConstantRange`、DX12 root constant）。
- **[per-function vs per-program 反射]**：spike 是 per-function 编译，`Shader::Descriptor.reflection` 需要合并后的整体 layout。当前无合并器 → 首个落地用单 entry（compute / 简单 VS+FS 共享 layout），合并器留后续。

## Migration Plan

1. 迁 `ShaderReflection.h` 到 `aurora/rhi`，改 `aurora/shader` 引用。
2. `Shader::Descriptor` 加 `reflection`；`ShaderBinaryProvider` 或 `Shader` 携带。
3. Vulkan：`VulkanShader::CreatePipelineLayout()` 由反射建 descriptor set layout + pipeline layout。
4. DX12：`D3D12Shader::Init` 由反射建 root signature；`D3D12PipelineState` 去外部 rootSig。
5. Metal：`MetalShader` 由反射建 argument buffer（stub 对齐）。
6. 测试：`SlangBackendTestCommon` 扩展为「编译 + 传反射 + 建 shader + 建 PSO」全链路；全量 build + ctest。
7. 待用户确认后 `openspec archive aurora-shader-derived-pso`。

## Open Questions

- push constants 反射字段放哪？（`ShaderReflection` 内加 `std::vector<PushConstantRange>`，复用 `rhi/Core.h` 的 `PushConstantRange`）
- per-function 反射如何合并成 `Shader` 级整体 layout？（首个落地用单 entry 规避，合并器后续）
- `ShaderReflection` 迁移到 rhi 后，`RgBlockDesc`/`ShaderCodeGen` 是否也要跟着迁到 rhi？（倾向保留在 shader，仅 `ShaderReflection` 下沉）
