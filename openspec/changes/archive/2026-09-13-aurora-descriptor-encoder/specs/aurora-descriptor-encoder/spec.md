# aurora-descriptor-encoder Specification

## Purpose

定义 RHI 层 `DescriptorEncoder` 抽象：类型安全的批量 descriptor set 写入接口，由各后端原生实现，取代 `ResourceUpdateInfo` + `Update(vector)`。

## ADDED Requirements

### Requirement: DescriptorEncoder 接口

`DescriptorEncoder` SHALL 是接口层抽象类，提供 `WriteBuffer` / `WriteImage` / `WriteSampler` 三个写入方法与 `End()` 提交方法：

- `WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0)`
- `WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0)`
- `WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0)`
- `End()` 把积累的写入 flush 到目标 descriptor set

`DescriptorEncoder` SHALL 不暴露 `ResourceUpdateInfo` 或任何通用中间结构；写入由后端直接积累为 native 结构。

#### Scenario: 链式批量写入

- **WHEN** 依次调用 `WriteBuffer` / `WriteImage` / `WriteSampler` 后调用 `End()`
- **THEN** 所有写入一次性提交到 descriptor set，后续 `BindResourceGroup` + Draw 可采样到对应资源

### Requirement: ResourceGroup::CreateEncoder 返回后端 encoder

`ResourceGroup` SHALL 提供 `CreateEncoder()`，返回绑定到该 group 的 descriptor set 的后端 `DescriptorEncoder*`。encoder SHALL 只作用于创建它的那个 group（Vulkan 目标 `VkDescriptorSet`；DX12 目标 root 参数）。

#### Scenario: 从 group 创建 encoder

- **WHEN** 调用 `group->CreateEncoder()`
- **THEN** 返回非空 `DescriptorEncoder*`，其 `End()` 写入该 group 的 descriptor set

### Requirement: 后端原生实现

Vulkan 后端 SHALL 提供 `VulkanDescriptorEncoder`：`Write*` 写入 RG 的持久化 packed buffer 并置 dirty，`End()` SHALL 在 dirty 时调用 `vkUpdateDescriptorSetWithTemplate`。`VulkanShader` SHALL 为每个 set 建 `VkDescriptorUpdateTemplate`（core 1.1，Aurora 要求 1.3 故始终可用）；template 创建失败时 `End()` 回退 `vkUpdateDescriptorSets`。

DX12 后端 SHALL 提供 `D3D12DescriptorEncoder`：按 reflection 类型分发——静态绑定写 descriptor table 的 CPU handle，动态绑定（`UNIFORM_BUFFER_DYNAMIC` / `STORAGE_BUFFER_DYNAMIC`）记录 buffer + baseOffset + range（bind 时由 root CBV/UAV 带 offset 重绑）。

Metal 后端 SHALL 提供 `MetalDescriptorEncoder`（接口就位，argument buffer 写入随 `aurora-resource-group Metal phase` 落地，暂为空实现 + TODO）。

#### Scenario: Vulkan 批量提交

- **WHEN** `VulkanDescriptorEncoder` 多个 `Write*` 后 `End()`
- **THEN** dirty 时调用一次 `vkUpdateDescriptorSetWithTemplate` 写入全部 binding；非 dirty 时跳过（无 flush）

#### Scenario: DX12 动态绑定分发

- **WHEN** 对 `UNIFORM_BUFFER_DYNAMIC` binding 调用 `WriteBuffer`
- **THEN** `D3D12DescriptorEncoder` 记录 buffer + baseOffset + range，不写 descriptor（bind 时走 root CBV）

### Requirement: DX12 shader-visible descriptor ring

D3D12 后端 SHALL 用 **CPU-only staging heap**（`D3D12_DESCRIPTOR_HEAP_FLAG_NONE`）作为描述符的持久 source of truth，并用 `ringSize` 张 **shader-visible heap**（每 in-flight frame 一张，默认 3）做 GPU 可见视图，两者 offset 1:1。

`D3D12DescriptorAllocator::BeginFrame(frameIndex)` SHALL 把当前帧设为 `frameIndex % ringSize`，由 `D3D12DeviceFrameContext::BeginFrame` 调用。`D3D12ResourceGroup::EnsureFrameCopy()` SHALL 在 `mDirty` 或当前帧未复制过时，用 `CopyDescriptorsSimple` 把 staging 区间拷到当前帧的 shader-visible heap。`D3D12GraphicsEncoder::BindResourceGroup` / `D3D12ComputeEncoder::BindResourceGroup` SHALL 在绑表前调用 `EnsureFrameCopy`。

#### Scenario: 每帧重写不覆盖 in-flight 帧

- **WHEN** 帧 N 写入描述符并提交，帧 N+1 `BeginFrame(N+1)` 后重写同一 RG
- **THEN** 帧 N+1 的 copy 落到不同的 shader-visible heap，不覆盖帧 N 仍在 GPU 读取的描述符

#### Scenario: 静态描述符跨帧复用

- **WHEN** RG 写一次后连续多帧 bind
- **THEN** 每帧第一次 bind 时 `EnsureFrameCopy` 从 staging 拷到当前帧 heap，GPU 读到正确描述符

### Requirement: 移除 ResourceUpdateInfo 与 Update(vector)

`ResourceGroup` SHALL 不再有 `Update(const std::vector<ResourceUpdateInfo>&)` 方法；`ResourceUpdateInfo` 与 `ResourceWriteKind` SHALL 从接口层移除。`DescriptorHeap::Update` SHALL 同步迁移到 encoder（bindless tier2，未启用时仅签名对齐）。

#### Scenario: Update 接口移除

- **WHEN** 调用 `group->Update({write})`
- **THEN** 编译错误（接口已移除，改用 `CreateEncoder()` + `Write*` + `End()`）
