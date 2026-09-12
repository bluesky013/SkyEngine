## 1. 删除 interface 层 PipelineLayout

- [x] 1.1 删除 `engine/aurora/rhi/interface/include/aurora/rhi/PipelineLayout.h`
- [x] 1.2 `Device.h` 移除 `CreatePipelineLayout` 纯虚声明，删除 `#include <aurora/rhi/PipelineLayout.h>`
- [x] 1.3 `PipelineState.h` 移除 `GraphicsPipeline::Descriptor::layout` 与 `ComputePipeline::Descriptor::layout`，删除对 `PipelineLayout.h` 的 include

## 2. Vulkan 后端清理

- [x] 2.1 删除 `VulkanPipelineLayout.h` / `VulkanPipelineLayout.cpp`
- [x] 2.2 `VulkanDevice.h/.cpp` 移除 `CreatePipelineLayout` 声明与实现
- [x] 2.3 `VulkanPipelineState.cpp` 删除 `desc.layout` 分支，统一 `vkShader->GetPipelineLayout()`；移除 `VulkanPipelineLayout.h` include
- [x] 2.4 确认 `VulkanShader::CreatePipelineLayout()` 仍产出 `VkPipelineLayout`（当前空 layout，行为不变）

## 3. DX12 / Metal 后端清理

- [x] 3.1 `D3D12Device.h` 移除 `CreatePipelineLayout` stub（内联声明，无 .cpp 实现）
- [x] 3.2 `MetalDevice.h` 移除 `CreatePipelineLayout` stub（内联声明，无 .cpp 实现）
- [x] 3.3 GLES 后端不存在（当前仅 dx12/interface/metal/test/vulkan），无需处理

## 4. 测试清理

- [x] 4.1 `ResourceGroupTest.cpp` 删除 `PipelineLayoutEmpty` / `PipelineLayoutWithGroups` / `PipelineLayoutTooManyGroupsRejected` 用例，移除 `#include <aurora/rhi/PipelineLayout.h>`；同时清理 `SlangBackendTestCommon.h` 的残留 `#include <aurora/rhi/PipelineLayout.h>`
- [x] 4.2 `MAX_RESOURCE_GROUPS` 常量随 `PipelineLayout.h` 删除（删除后无剩余消费方；`aurora-resource-group` 落地时如需上限再放 `ResourceGroup.h`）

## 5. 构建与验证

- [x] 5.1 全量 `cmake --build` 通过（interface + vulkan + dx12 + metal + test）
- [x] 5.2 ctest 全绿（AuroraRHITest 114、AuroraShaderTest 17、AuroraPipelineTest 7、AuroraCoreTest 7）
- [x] 5.3 变更以删除为主，保留原格式；clang-format 不可用（环境无 clang-format 二进制），改动已符合仓库风格

## 6. 收尾

- [ ] 6.1 待用户确认后 `openspec archive aurora-shader-derived-layout` 归档本 change
