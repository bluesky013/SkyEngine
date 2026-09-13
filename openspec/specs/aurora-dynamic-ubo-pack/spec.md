# aurora-dynamic-ubo-pack Specification

## Purpose
TBD - created by archiving change aurora-dynamic-ubo-pack. Update Purpose after archive.
## Requirements
### Requirement: dynamic UBO stable binding 契约

Batch tier 的 dynamic UBO descriptor SHALL 以 `offset=0, range=blockSize` 绑定（`blockSize` 为该 RG 对应 shader block 的固定大小，std140 后 16B 对齐），每帧绑定一次（非 per-object）；帧内每 draw 仅通过 dynamic offset 选择 pack buffer 内的数据窗口。

`ResourceUpdateInfo::bufferRange` SHALL 在 DYNAMIC 类型绑定中为 `blockSize`（MUST 非 0）；Vulkan 后端对 `UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC` + `range==0` MUST 在 debug 断言 / 打 warning（不得静默退化为 `VK_WHOLE_SIZE`）。

#### Scenario: 帧内只写一次 descriptor

- **WHEN** batch RG 对本帧 pack buffer `Update({binding=0, BUFFER, buffer=packBuffer, offset=0, range=blockSize})` 一次，帧内多次 draw 用不同 dynamic offset
- **THEN** 帧内 descriptor set 无需再次 `Update`；每次 draw 只改 dynamic offset 即可正确采样到各自数据窗口

#### Scenario: DYNAMIC 类型 range 为 0 拒绝

- **WHEN** 对 `UNIFORM_BUFFER_DYNAMIC` binding 写入 `bufferRange=0`
- **THEN** Vulkan 后端 debug 断言 / 打 warning；release 不静默用 `VK_WHOLE_SIZE`

### Requirement: BatchAllocator 线性分配器，in-flight 由 FrameContext 维护

`BatchAllocator` SHALL 是纯线性分配器：`Init(device, capacity)` 创建单 host-visible buffer，`Allocate(size) -> offset` 在当前 buffer 内按 `DeviceCapability::minUniformBufferOffsetAlignment` 对齐分配，耗尽返回 `UINT32_MAX`；`Write(offset, data, size)` 写入映射区；`Reset()` 清 cursor。

`BatchAllocator` SHALL 不感知帧：无 `inflightNum`、无 `BeginFrame(frameIndex)`。in-flight 安全 SHALL 由 `DeviceFrameContext` 维护 `inflightNum` 个 pack buffer（每 in-flight frame 一个），帧完成后回收并 `Reset()` 复用。

#### Scenario: 线性分配

- **WHEN** 对单 buffer 连续 `Allocate(64)` 两次
- **THEN** 第二次 offset = 首次 offset + 对齐后的 64（偏移按 `minUniformBufferOffsetAlignment` 对齐）

#### Scenario: 分配耗尽

- **WHEN** buffer 剩余空间小于请求 size
- **THEN** `Allocate` 返回 `UINT32_MAX`

#### Scenario: Reset 复用

- **WHEN** `Reset()` 后再次 `Allocate`
- **THEN** 从 buffer 起始重新分配

#### Scenario: in-flight 由 FrameContext 维护

- **WHEN** FrameContext 持有 `inflightNum` 个 allocator，帧完成回收后 `Reset` 复用
- **THEN** 复用不覆盖仍在 GPU 读取的帧数据（回收由 frame fence 驱动）

### Requirement: offset 对齐取自 device capability

`DeviceCapability` SHALL 含 `minUniformBufferOffsetAlignment`（默认 256）。Vulkan / DX12 / Metal 后端 SHALL 在 `UpdateDeviceCaps()` 中从原生 limits 填真实值（Vulkan `minUniformBufferOffsetAlignment`、DX12 `D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT`、Metal `minConstantBufferAlignmentBytes`）。

`BatchAllocator` SHALL 用 `device->GetCapability().minUniformBufferOffsetAlignment` 对齐分配，MUST 不硬编码 256。

#### Scenario: 对齐由 caps 决定

- **WHEN** device 上报 `minUniformBufferOffsetAlignment = 16`，`Allocate(64)` 连续两次
- **THEN** 第二次返回的 offset 是 16 的倍数（而非 256 的倍数）

### Requirement: pack writer 返回 dynamic offset

pipeline 层 SHALL 提供 pack writer，把 per-object 结构体写入已分配 block 并返回该 block 的 pack offset；返回的 offset SHALL 可直接作为 `DrawItem::batchDynamicOffset` 传给 executor。

pack writer SHALL 用 `RgBlockDesc`/codegen struct 的 block size 校验（`static_assert(sizeof)`），MUST 不感知 rhi 层字节分配的内部实现。

#### Scenario: 写入并返回 offset

- **WHEN** 调用 pack writer 写入结构体 `T`（`sizeof(T)=64`）
- **THEN** 返回值为该帧 allocator 分配的、按 caps 对齐的绝对 offset，且该 offset 处的映射内存已写入 `T` 的内容

#### Scenario: offset 直通 executor

- **WHEN** pack writer 返回的 offset 写入 `DrawItem.batchDynamicOffset` 后 `Compile`+`Execute`
- **THEN** executor `BindResourceGroup(2, rg, 1, &offset)` 使用该 offset 作为 dynamic offset

