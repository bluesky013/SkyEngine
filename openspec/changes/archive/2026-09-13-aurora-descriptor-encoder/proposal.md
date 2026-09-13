## Why

当前 `ResourceGroup::Update(const std::vector<ResourceUpdateInfo>&)` 是粗糙的 descriptor 写入 API：

- **手动 tag union**：`ResourceUpdateInfo` 用一个 `kind` + 三组字段（buffer / image / sampler）混装，调用方要自己记得设 `kind` 和对应字段，类型不安全、易出错。
- **每次堆分配**：每次 `Update` 都要构造 `std::vector`，即使只写一个 binding。
- **不流畅**：没有链式/批量写法，写多个 binding 要手拼 vector。

需要把 descriptor 写入封装成 RHI 层 encoder，由各后端原生实现，做到类型安全、批量提交、避免每帧分配。

## What Changes

- 新增接口层 **`DescriptorEncoder`**（`aurora/rhi/DescriptorEncoder.h`）：`WriteBuffer` / `WriteImage` / `WriteSampler` + `End()` 批量提交
- **`ResourceGroup::CreateEncoder()`** 返回后端 encoder（仿 `CommandBuffer::CreateGraphicsEncoder` 模式）
- **移除 `ResourceUpdateInfo` 与 `ResourceGroup::Update(vector)`**：不再有通用中间结构，后端各自原生积累写入
  - Vulkan：`VulkanDescriptorEncoder` 积累 `VkWriteDescriptorSet`，`End()` → `vkUpdateDescriptorSets`
  - DX12：`D3D12DescriptorEncoder` 直接写 root CBV / descriptor table
  - Metal：`MetalDescriptorEncoder` 写 argument buffer（RG 仍是 stub，随 `aurora-resource-group Metal phase` 落地）
- `DescriptorHeap::Update` 同步迁移到 encoder（bindless tier2，当前未启用）
- 迁移现有调用方：`GlobalRenderResources`、`BatchPackWriter` 相关、`ResourceGroupTest` / `ResourceTiersTest`

## Capabilities

### New Capabilities

- `aurora-descriptor-encoder`: RHI 层 DescriptorEncoder 抽象 + 各后端原生实现 + ResourceGroup::CreateEncoder 批量提交语义

### Modified Capabilities

- `aurora-resource-binding`: 「ResourceGroup::Update 批量写入资源」需求变更——由 `Update(vector<ResourceUpdateInfo>)` 改为 `CreateEncoder()` + `DescriptorEncoder`（`ResourceUpdateInfo` 移除）

## Impact

- **接口层**：新增 `aurora/rhi/DescriptorEncoder.h`；`ResourceGroup.h` 加 `CreateEncoder()`、删 `Update()`；`ResourceGroup.h` / `DescriptorHeap.h` 删 `ResourceUpdateInfo`
- **后端**：Vulkan / DX12 / Metal 各新增 `*DescriptorEncoder`；`*ResourceGroup::Update` 逻辑迁移到 encoder
- **调用方**：`GlobalRenderResources::Init`、`pipeline/test/ResourceTiersTest.cpp`、`rhi/test/ResourceGroupTest.cpp` 迁移到 encoder
- **依赖**：与刚落地的 `aurora-dynamic-ubo-pack` 重叠（stable binding 测试里用了 `ResourceUpdateInfo`），需一并迁移
