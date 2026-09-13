## Why

Batch tier（set 2）目前用 `BatchAllocator` 管理 per-frame dynamic UBO，但它既不安全也不具备稳定的绑定语义：

- **单 buffer + 帧末 `Reset()`**：CPU 在帧 N+1 覆盖数据时，GPU 可能仍在读帧 N 的数据（多帧 in-flight 时数据竞争）
- **对齐硬编码 256**：没有从 device caps 查询 `minUniformBufferOffsetAlignment`
- **range 绑定错误**：`ResourceUpdateInfo.bufferRange == 0` 会落到 `VK_WHOLE_SIZE`，对 `UNIFORM_BUFFER_DYNAMIC` 是非法绑定（dynamic UBO 必须绑定固定窗口 ≤ block size）

要让 batch 渲染正确且高效，需要一套成型的 dynamic UBO pack 设计：**每帧绑定 `0 ~ blockSize` 的固定窗口（descriptor 稳定），帧内只根据 pack offset 更新 dynamic offset**，并支持多帧 in-flight。

## What Changes

- 明确 **stable binding 契约**：dynamic UBO 每帧以 `offset=0, range=blockSize` 绑定一次（非 per-object），帧内只有 per-draw 的 dynamic offset 变化
- `BatchAllocator` 保持**纯线性分配器**（对齐取自 caps），去掉 frame index 绑定；in-flight 安全由 `DeviceFrameContext` 维护 **per-inflight-frame 的 pack buffer（packed pool）**
- `DeviceCapability` 新增 `minUniformBufferOffsetAlignment`；Vulkan / DX12 / Metal 在 `UpdateDeviceCaps` 里填真实值
- 用 capability 值替换硬编码的 `OFFSET_ALIGNMENT = 256`
- pipeline 层新增 **pack writer API**：把 per-object uniform 结构写入已分配 block 并返回 dynamic offset
- `DrawItem.batchDynamicOffset` 语义澄清为「pack allocator 产出的 pack offset」
- Vulkan 后端动态 UBO 以固定 `range = blockSize` 绑定（不再 `VK_WHOLE_SIZE`）
- batch RG 的创建/绑定**不在本 change 范围**：由各特性（如材质→shader）自行创建，本 change 只提供 allocator + stable binding 契约 + 测试用例

## Capabilities

### New Capabilities

- `aurora-dynamic-ubo-pack`: dynamic UBO 的 pack allocator、stable binding 契约、packed pool（per-inflight-frame 由 FrameContext 维护）、pack offset → dynamic offset 映射

### Modified Capabilities

- `aurora-resource-tiers`: 「Batch tier dynamic UBO」需求变更——对齐由 device caps 决定（不再硬编码 256）、由纯线性 pack allocator 管理（in-flight 由 FrameContext 维护，不再单 buffer 帧末 reset）、`batchDynamicOffset` 语义改为 pack offset

## Impact

- **接口层**：`aurora/rhi/Device.h`（`DeviceCapability` 加字段）、`aurora/rdg/BatchAllocator.h`（线性分配器 + 对齐）
- **后端**：Vulkan / DX12 / Metal 的 `UpdateDeviceCaps` 填 `minUniformBufferOffsetAlignment`；`VulkanResourceGroup::Update` 对 `UNIFORM_BUFFER_DYNAMIC` 绑定固定 range
- **pipeline 层**：新增 pack writer（per-object 写入 + offset 返回）；`GlobalRenderResources` 不受影响
- **测试**：`ResourceTiersTest` 扩展（对齐、线性分配/耗尽、stable binding range）
- **依赖**：per-inflight-frame 维护随 `aurora-renderer` / `DeviceFrameContext` 落地（`aurora-frame-context` 已提供帧生命周期）；batch RG 创建由各特性完成
