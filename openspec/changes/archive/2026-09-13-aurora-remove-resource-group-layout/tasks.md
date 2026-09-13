## 1. 接口层

- [x] 1.1 `ResourceGroup.h`：删除 `ResourceGroupLayout` 类；`ResourceGroup::Descriptor` 改为 `{Shader *shader, uint32_t set}`；删除 `ResourceWriteKind::COMBINED_IMAGE_SAMPLER` 枚举值
- [x] 1.2 `Core.h`：删除 `DescriptorType` / `DescriptorBindingFlagBit` / `DescriptorBindingFlags` 枚举
- [x] 1.3 `ShaderReflection.h`：`ShaderResourceType` 加 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` 变体
- [x] 1.4 `Device.h`：删除 `CreateResourceGroupLayout` 纯虚声明
- [x] 1.5 `Shader.h`：reflection null 语义注释（MUST 非 null）
- [x] 1.6 全仓库搜 `ResourceGroupLayout` / `CreateResourceGroupLayout` / `DescriptorType` / `DescriptorBindingFlag` 残留引用并清理

## 2. Vulkan 后端

- [x] 2.1 `VulkanShader`：修复 per-set `VkDescriptorSetLayout` 的 set 索引映射（改 `std::map<uint32_t, VkDescriptorSetLayout>`），暴露 `VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const`
- [x] 2.2 `VulkanShader::Init`：reflection null 校验（null 报错，empty 合法）；`FromShaderResourceType` 映射 DYNAMIC 变体（迁到 `VulkanConversion`）
- [x] 2.3 `VulkanResourceGroup`：`Init` 改为 `{shader, set}`，用 `VulkanShader::GetDescriptorSetLayout(set)` 分配 descriptor set；内部维护 binding→type 映射（从 reflection 快照）；持 `CounterPtr<VulkanShader>`
- [x] 2.4 删除 `VulkanResourceGroupLayout.{h,cpp}` 与 `VulkanConversion::FromDescriptorType` / `FromDescriptorBindingFlags`
- [x] 2.5 `VulkanDevice`：删除 `CreateResourceGroupLayout` 实现；`CreateResourceGroup` 改签名

## 3. DX12 后端

- [x] 3.1 `D3D12Shader`：暴露 `const ShaderReflection &GetReflection() const`；`Init` reflection null 校验
- [x] 3.2 `D3D12RootSignature`：`RootSignatureDescriptorRange.type` 改 `ShaderResourceType`，`ToRangeType` 收 `ShaderResourceType`（含 DYNAMIC 变体映射）
- [x] 3.3 `D3D12ShaderFunction`：删除 `FromShaderResourceType`（返回 `DescriptorType`）中间转换
- [x] 3.4 `D3D12ResourceGroup`：`Init` 改为 `{shader, set}`，从 reflection 计算 cbvSrvUav/sampler 数量与 per-binding offset；空 set 返回 nullptr
- [x] 3.5 删除 `D3D12ResourceGroupLayout.{h,cpp}` 与 `D3D12Conversion::FromDescriptorType`
- [x] 3.6 `D3D12Device`：删除 `CreateResourceGroupLayout` 实现；`CreateResourceGroup` 改签名

## 4. Metal 后端

- [x] 4.1 `MetalDevice`：删除 `CreateResourceGroupLayout` stub；`CreateResourceGroup` 保持 stub（`{shader, set}` 签名）
- [ ] 4.2 `MetalResourceGroup`：从 reflection 派生 argument buffer 布局，实现 `CreateResourceGroup`（后续 change，需 macOS 验证）
- [ ] 4.3 Metal dynamic offset：`setVertexBuffer:offset:` / `setFragmentBuffer:offset:` / `setBuffer:offset:`（配合 batch tier，后续 change）

## 5. 上层 pipeline / shader

- [x] 5.1 `RgBlockDesc.cpp`：删除 `ToLayoutDescriptor`（`ComputeFieldOffsets` 保留，供 UBO size 计算）
- [x] 5.2 dummy shader：`GlobalRenderResources::Init(Device*, Shader*)` 接收外部 global shader（测试用 `ShaderCompilerSlang` 编译 dummy shader 验证）；生产侧"内部编译 dummy shader"留待 codegen
- [x] 5.3 `GlobalRenderResources`：改为持有 shader 引用，`CreateResourceGroup({shader, 0})`
- [x] 5.4 `PipelinePass`：`RebuildPassResources` 用 `GetPassShader()`（新增虚函数，默认 nullptr）+ set 1 创建 RG，删除 `ToLayoutDescriptor` 调用

## 6. 测试

- [x] 6.1 `ResourceGroupTest`（Vulkan + DX12）：改为"构造 shader（最小 SPIR-V + reflection）→ `{shader, set}` 创建 RG"；删除 COMBINED 用例
- [x] 6.2 补 set 索引空洞用例：shader 含 set 2 时 `CreateResourceGroup({shader, 2})` 成功
- [x] 6.3 补 reflection null 拒绝 + 空 reflection 语义用例（空 reflection shader 合法，不存在的 set 返回 nullptr）
- [x] 6.4 `RgBlockDescTest`：删除 `ToLayoutDescriptor` 相关用例
- [x] 6.5 `ResourceTiersTest` / `ShaderTest` / `EncoderTest` / `SlangBackendTestCommon`：适配 shader reflection null 校验 + `{shader, set}` 创建路径

## 7. 收尾 / 文档

- [x] 7.1 构建 Vulkan + DX12 + Metal（接口）+ Pipeline + Shader + 测试目标，修复编译错误
- [x] 7.2 全量回归：AuroraRHITest 123/123、AuroraShaderTest 40/40、AuroraPipelineTest 10/10 通过
- [x] 7.3 `engine/aurora/AGENTS.md`：更新 ResourceGroup 创建说明（`{shader, set}` + reflection 派生 + 删 `DescriptorType`）与 change 路线表
- [x] 7.4 archive：已归档（Metal ResourceGroup + dynamic offset 留待后续 change）
