## ADDED Requirements

### Requirement: D3D12 tier2 bindless DescriptorHeap

`D3D12Device::CreateDescriptorHeap` SHALL 返回 `D3D12DescriptorHeap : DescriptorHeap`（而非 `nullptr`，当能力门开启时）。该对象 SHALL 内部持有两个 shader-visible backing heap（resource = `CBV_SRV_UAV`、sampler），`Allocate` SHALL 按 per-type 索引从 free list 分配并返回 `{texFirst/texCount, bufFirst/bufCount, smpFirst/smpCount}`，`Free` SHALL 归还并合并相邻区间。D3D12 descriptor 写入是即时模型：SHALL 经 `D3D12DescriptorHeap::CreateHeapEncoder(allocation)` 返回的 heap-bound `DescriptorEncoder` 立即写入；基类 `Update(allocation, encoder)` SHALL 为对称性 no-op。

#### Scenario: 分配 per-type 索引

- **WHEN** 申请 2 张纹理 + 1 个 buffer
- **THEN** `Allocate` 返回纹理索引连续、buffer 索引独立的 `Allocation`

#### Scenario: 释放后可复用

- **WHEN** `Allocate` 后 `Free` 同一 `Allocation`，再次 `Allocate` 同规模
- **THEN** 新分配成功且区间不重叠

#### Scenario: heap encoder 写入堆

- **WHEN** 用 `CreateHeapEncoder(alloc)` 的 `WriteImage/WriteBuffer/WriteSampler` 写入 descriptor
- **THEN** 对应索引段的 heap descriptor 被立即填充；`Update` 为 no-op

### Requirement: D3D12 BindDescriptorHeap 生效

`D3D12GraphicsEncoder::BindDescriptorHeap` 与 `D3D12ComputeEncoder::BindDescriptorHeap` SHALL 调用 `SetDescriptorHeaps` 绑定 heap 的 resource + sampler 两个 shader-visible heap；SHALL NOT 为空实现。

#### Scenario: 绑定 bindless heap

- **WHEN** 在 encoder 上 `BindDescriptorHeap(heap)`
- **THEN** 后续 draw/dispatch 的 shader 可经 heap index 访问 descriptor

### Requirement: D3D12 descriptor heap 能力门基于 Shader Model 6.6

`D3D12Device::UpdateDeviceCaps` SHALL 通过 `CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL)` 判定 Shader Model >= 6.6，并写入 `Device::GetFeature().descriptorHeap`。能力不可用时 `CreateDescriptorHeap` SHALL 返回 `nullptr`，调用方退化为 tier1。Vulkan 后端未实现 tier2（`VK_EXT_descriptor_heap`），`feature.descriptorHeap` SHALL 为 `false`。

#### Scenario: SM6.6 设备开启

- **WHEN** 设备支持 Shader Model 6.6
- **THEN** 设备标记 descriptor heap 可用，`CreateDescriptorHeap` 返回实例

#### Scenario: 低版本设备退化

- **WHEN** 设备最高 Shader Model < 6.6
- **THEN** `CreateDescriptorHeap` 返回 `nullptr`
