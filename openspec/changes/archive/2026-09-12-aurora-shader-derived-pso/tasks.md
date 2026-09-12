## 1. 反射结构下沉到 RHI

- [x] 1.1 迁移 `ShaderReflection.h`（`ShaderResource`/`ShaderBlockLayout`/`ShaderResourceType`/`ShaderScalarType`/`ShaderTypeKind`）到 `aurora/rhi/interface/include/aurora/rhi/ShaderReflection.h`
- [x] 1.2 删除 `aurora/shader/include/aurora/shader/ShaderReflection.h`，改 `ShaderCompilerSlang`/`ShaderCodeGen`/`ReflectionValidation` 的 include 为 `<aurora/rhi/ShaderReflection.h>`
- [x] 1.3 `Shader.h` 的 `Shader::Descriptor` 增加 `const ShaderReflection *reflection = nullptr`；`ShaderReflection` 补充 `std::vector<PushConstantRange> pushConstants`（复用 `rhi/Core.h` 的 `PushConstantRange`）
- [x] 1.4 全量编译通过（interface + shader 模块）

## 2. Vulkan layout 由反射派生

- [x] 2.1 `VulkanShader::Init` 保存 `desc.reflection`（复制到成员）
- [x] 2.2 `VulkanShader::CreatePipelineLayout()` 由反射构建：按 set 分组生成 `VkDescriptorSetLayout`（binding/descriptorType/count/stageFlags），push constants → `VkPushConstantRange`，最终 `VkPipelineLayout`
- [x] 2.3 验证：`SlangToRhiComputePipeline`（带 UBO 的 compute shader）建 `VkPipelineLayout` + compute PSO 成功

## 3. DX12 root signature 由反射派生

- [x] 3.1 `D3D12Shader::Init` 由 `desc.reflection` 填 `RootSignatureDescriptor`（set → descriptor table，range 来自 binding/type/stage），调用 `D3D12RootSignature::Init`；顺带修复 compute 的 `psOrCs` 赋值（原为 `desc.ps`，compute 时为空）
- [x] 3.2 `D3D12ComputePipeline::Init(const Descriptor&)` 去掉外部 `rootSig` 参数，统一 `d3dShader->GetRootSignature()`
- [x] 3.3 `D3D12Device::CreatePipelineState` 由 stub 改为真实创建 `D3D12GraphicsPipeline`/`D3D12ComputePipeline`

## 4. Metal argument buffer（stub 对齐）

- [x] 4.1 `MetalShader` 保存 `desc.reflection`，`Init` 预留 argument buffer 派生（当前 stub 不崩）

## 5. PSO 全链路测试

- [x] 5.1 扩展 `SlangBackendTestCommon`：新增 `kSlangComputeCs`（带 UBO ParameterBlock）+ `RunSlangToRhiComputePipelineTest`（编译 → 传反射 → 建 shader → 建 compute PSO）
- [x] 5.2 Vulkan：`SlangVulkanTest.SlangToRhiComputePipeline` 走全链路（VkPipelineLayout 由反射派生）
- [x] 5.3 DX12：`SlangD3D12Test.SlangToRhiComputePipeline` 走全链路（root signature 由反射派生，PSO 创建成功）
- [x] 5.4 全量 build + ctest 全绿（AuroraShaderTest 19、AuroraRHITest 114、AuroraPipelineTest 7）

## 6. 收尾

- [x] 6.1 变更以新增/精简为主，保留原格式；clang-format 不可用（环境无二进制），改动已符合仓库风格
- [ ] 6.2 待用户确认后 `openspec archive aurora-shader-derived-pso` 归档本 change
