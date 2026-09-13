## 1. 接口层（DeviceCapability + BatchAllocator）

- [x] 1.1 `aurora/rhi/Device.h`：`DeviceCapability` 新增 `uint32_t minUniformBufferOffsetAlignment = 256`
- [x] 1.2 `aurora/rdg/BatchAllocator.h`：`Init` 保持 `Init(device, capacity)` 签名，去掉 `OFFSET_ALIGNMENT` 常量，保留 `Allocate/Write/Reset`（不新增 frame index API）
- [x] 1.3 `aurora/rdg/BatchAllocator.cpp`：实现线性分配（对齐从 `device->GetCapability().minUniformBufferOffsetAlignment` 取、`Allocate` 耗尽返回 `UINT32_MAX`、`Write` 按 offset 落映射区、`Reset` 清 cursor）

## 2. 后端 capability 填充

- [x] 2.1 Vulkan `UpdateDeviceCaps`：从 `VkPhysicalDeviceLimits::minUniformBufferOffsetAlignment` 填 `minUniformBufferOffsetAlignment`
- [x] 2.2 DX12 `UpdateDeviceCaps`：填 `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`（256）
- [x] 2.3 Metal `UpdateDeviceCaps`：填 `[device minConstantBufferAlignmentBytes]`

## 3. Vulkan stable binding

- [x] 3.1 `VulkanResourceGroup::Update`：对 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` + `bufferRange==0` 做 debug assert / 打 warning（不再静默用 `VK_WHOLE_SIZE`）
- [x] 3.2 在测试中手工构造 batch RG（shader 带 `UNIFORM_BUFFER_DYNAMIC` block）显式写 `offset=0, range=blockSize`（`bufferRange` 非 0），验证 stable binding 契约（batch RG 创建本身由各特性/材质负责，本 change 不实现）

## 4. DX12 接线

- [x] 4.1 `D3D12ResourceGroup`：记录 DYNAMIC binding 的 range（用于校验），不改 root CBV `baseOffset + offset` 机制
- [x] 4.2 确认 stable binding 下 `baseOffset=0`、`offset=packOffset` 语义等价（`D3D12GraphicsEncoder::BindResourceGroup` 已就位）

## 5. pipeline 层 pack writer

- [x] 5.1 新增 pack writer（pipeline 层）：`Pack(BatchAllocator&, const T&) -> uint32_t`（`Allocate(sizeof(T))` + `Write` + 返回 offset），用 codegen struct `static_assert(sizeof)` 校验
- [x] 5.2 确认返回 offset 可直接写入 `DrawItem::batchDynamicOffset`
- [x] 5.3 文档化 per-inflight-frame 维护归属：pack buffer 的回收随 `aurora-renderer` / `DeviceFrameContext` 落地（本 change 只交付 allocator 语义，不实现回收/扩容）

## 6. 测试

- [x] 6.1 扩展 `ResourceTiersTest::BatchAllocatorAllocWriteReset`：改为新 `Init(device, capacity)` 签名，覆盖线性分配（第二次 offset 对齐）、耗尽返回 `UINT32_MAX`、`Reset()` 后从起始复用
- [x] 6.2 新增对齐测试：caps 上报 `minUniformBufferOffsetAlignment` 时分配 offset 为该值倍数（非硬编码 256）
- [x] 6.3 新增 stable binding 测试：手工构造 batch RG（shader 带 `UNIFORM_BUFFER_DYNAMIC` block，测试中把 reflection 类型改为 DYNAMIC），`Update({offset=0, range=blockSize})` 成功（不触发 assert），验证 stable binding 契约
- [x] 6.4 新增 DYNAMIC range==0 拒绝测试：对 `UNIFORM_BUFFER_DYNAMIC` 写 `bufferRange=0` 触发 assert/warning（debug 用 death test，release 验证不崩溃）

## 7. 收尾 / 文档

- [x] 7.1 更新 `engine/aurora/AGENTS.md`：记录 dynamic UBO stable binding 契约（`offset=0, range=blockSize`，每帧一次）、`BatchAllocator` 纯线性分配（无 frame index）、per-inflight-frame 由 FrameContext 维护、batch RG 由各特性（材质→shader）创建
- [x] 7.2 跑 `AuroraRHITest` / `AuroraPipelineTest` 相关测试全绿（至少 Vulkan）
- [x] 7.3 确认 DX12 / Metal 编译通过（capability 字段新增不破坏 ABI 兼容旧调用方）
- [ ] 7.4 archive 本 change：`openspec archive aurora-dynamic-ubo-pack`（待用户确认）
