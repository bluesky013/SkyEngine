## 1. 接口层

- [x] 1.1 新增 `aurora/rhi/DescriptorEncoder.h`：抽象类 `WriteBuffer` / `WriteImage` / `WriteSampler` + `End()`
- [x] 1.2 修改 `aurora/rhi/ResourceGroup.h`：加 `virtual std::unique_ptr<DescriptorEncoder> CreateEncoder() = 0`；删 `Update(vector)`、`ResourceUpdateInfo`、`ResourceWriteKind`
- [x] 1.3 修改 `aurora/rhi/DescriptorHeap.h`：`Update` 签名从 `vector<ResourceUpdateInfo>` 改为 `DescriptorEncoder &`（bindless tier2 未启用，仅签名对齐）
- [x] 1.4 `aurora/rhi/Device.h` 顶部聚合 include 加 `DescriptorEncoder.h`（如适用）

## 2. Vulkan 后端

- [x] 2.1 `VulkanFunctions` 加载 `vkCreateDescriptorUpdateTemplate` / `vkDestroyDescriptorUpdateTemplate` / `vkUpdateDescriptorSetWithTemplate`（core 1.1，`vkGetDeviceProcAddr` 直接可拿）
- [x] 2.2 修改 `VulkanShader::CreatePipelineLayout`：为每个 set 建 `VkDescriptorUpdateTemplate`（`VkDescriptorUpdateTemplateEntry` 指向 packed buffer），加 `GetDescriptorUpdateTemplate(set)`
- [x] 2.3 修改 `VulkanResourceGroup`：持持久化 `mWriteInfos`（`DescriptorWriteInfo` union 数组，size = 总 descriptor 数）+ `mDirty`；加 `CreateEncoder()`；删 `Update`
- [x] 2.4 新增 `VulkanDescriptorEncoder.h/.cpp`：thin facade，`Write*` 写 `mWriteInfos[slot]` + 置 dirty；`End()` dirty 时 `vkUpdateDescriptorSetWithTemplate`（template 为 null 回退 `vkUpdateDescriptorSets`）
- [x] 2.5 保留 `aurora-dynamic-ubo-pack` 的 DYNAMIC `range==0` assert/warning 语义在 encoder 内

## 3. DX12 后端

- [x] 3.1 新增 `D3D12DescriptorEncoder.h/.cpp`：按 reflection 类型分发——静态写 descriptor table CPU handle，动态（`*_DYNAMIC`）记录 buffer + baseOffset + range；`End()` 提交
- [x] 3.2 修改 `D3D12ResourceGroup`：加 `CreateEncoder()`；删 `Update`
- [x] 3.3 shader-visible ring：`D3D12DescriptorAllocator` 改 CPU-only staging + `ringSize` 张 shader-visible heap + `BeginFrame`/`Copy*`；`D3D12ResourceGroup::EnsureFrameCopy`；`BindResourceGroup` 绑前 copy；`D3D12DeviceFrameContext::BeginFrame` 驱动 ring 轮换

## 4. Metal 后端

- [x] 4.1 新增 `MetalDescriptorEncoder.h`（header-only stub）：接口就位，argument buffer 写入留 TODO（随 `aurora-resource-group Metal phase` 落地）
- [x] 4.2 `MetalResourceGroup` 不存在（`MetalDevice` 无 `CreateResourceGroup`），无接线，随 `aurora-resource-group Metal phase` 落地

## 5. 迁移调用方

- [x] 5.1 `GlobalRenderResources::Init`：`mGroup->Update({write})` → `CreateEncoder()` + `WriteBuffer` + `End()`
- [x] 5.2 `rhi/test/ResourceGroupTest.cpp`：`UpdateUniformBuffer` / `UpdateDynamicUniformBuffer` 等迁移到 encoder
- [x] 5.3 `pipeline/test/ResourceTiersTest.cpp`：stable binding / range==0 测试迁移到 encoder

## 6. 测试

- [x] 6.1 新增 encoder 批量写入测试：`WriteBuffer` + `WriteImage` + `WriteSampler` + `End()` 后不报错
- [x] 6.2 新增 DX12 动态绑定分发测试：`UNIFORM_BUFFER_DYNAMIC` 走 root CBV 记录、不写 descriptor
- [x] 6.3 保留 DYNAMIC `range==0` 拒绝测试（迁移到 encoder 路径）

## 7. 收尾 / 文档

- [x] 7.1 更新 `engine/aurora/AGENTS.md`：记录 DescriptorEncoder 批量写入契约、移除 ResourceUpdateInfo
- [x] 7.2 跑 `AuroraRHITest` / `AuroraPipelineTest` 全绿（至少 Vulkan）
- [x] 7.3 确认 DX12 编译通过（Metal 无法本机验证，注明）
- [ ] 7.4 archive 本 change：`openspec archive aurora-descriptor-encoder`（待用户确认）

## 后续（未完成，暂不阻塞本 change）

- [ ] D3D12 `EnsureFrameCopy` / `CopyDescriptorsSimple` copy 路径测试：需 D3D12 compute PSO（root signature）+ command buffer + `BindResourceGroup` harness，留待后续补充（当前只测了 encoder 写入不崩溃 + frame ring 轮换）。
