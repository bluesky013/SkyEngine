## ADDED Requirements

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
