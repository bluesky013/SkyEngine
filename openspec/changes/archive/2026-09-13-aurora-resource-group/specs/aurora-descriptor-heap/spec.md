## ADDED Requirements

### Requirement: tier 模型——tier1 pool，tier2 heap（typed 与 untyped 都走 heap）

- **tier1**（无 `VK_EXT_descriptor_heap`）：typed（`layout(set, binding)`）SHALL 走 `VkDescriptorPool` + `VkDescriptorSet`；untyped SHALL NOT 可用。
- **tier2**（有 `VK_EXT_descriptor_heap`）：typed SHALL 走 heap + `VkShaderDescriptorSetAndBindingMappingInfoEXT`，untyped SHALL 走 heap + untyped pointer（`VK_KHR_shader_untyped_pointers` + `SPV_EXT_descriptor_heap`）；**两者 SHALL 走同一套 heap**，不再用 pool。

#### Scenario: tier1 仅 typed

- **WHEN** 设备无 `VK_EXT_descriptor_heap`
- **THEN** typed `ResourceGroup` 走 pool；`CreateDescriptorHeap` 返回 nullptr

#### Scenario: tier2 typed + untyped 都走 heap

- **WHEN** 设备有 `VK_EXT_descriptor_heap`
- **THEN** typed `ResourceGroup` 与 untyped `DescriptorHeap` 都可用，都落在 heap

### Requirement: DescriptorHeap 一个对象管两个 backing buffer

`DescriptorHeap` SHALL 内部持有两个 backing buffer（resource heap + sampler heap），`Allocate` SHALL 按 per-type 索引分配，`Free` SHALL 归还，`Update` SHALL 把 descriptor 写入索引段（经 `vkWriteResourceDescriptorsEXT`）。

#### Scenario: 分配 per-type 索引

- **WHEN** 一个 material 需 2 纹理 + 1 buffer
- **THEN** `Allocate` 返回 `{texFirst, texCount=2, bufFirst, bufCount=1, ...}`，纹理与 buffer 索引各自连续

### Requirement: typed ResourceGroup 在 tier2 走 heap 映射

typed `ResourceGroup` 接口 SHALL 不变；tier2 下 backend SHALL 用 heap region + `VkShaderDescriptorSetAndBindingMappingInfoEXT` 实现，调用方 SHALL NOT 感知后端是 pool 还是 heap。

#### Scenario: typed 在 tier2 无感

- **WHEN** tier2 设备上创建并绑定 `ResourceGroup`
- **THEN** 调用方代码与 tier1 一致，backend 内部走 heap 映射

### Requirement: mapping source 先只支持 HEAP_WITH_PUSH_INDEX

heap 模型的逐 draw 索引 SHALL 先只支持 `VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT`（material index 走 push data）；`CONSTANT_OFFSET` / `INDIRECT_INDEX` / `INDIRECT_INDEX_ARRAY` / inline 系列 / shader-record SHALL 留后续。

#### Scenario: push index 逐 draw 索引

- **WHEN** bind heap 后逐 draw 经 push data 传入 index
- **THEN** shader 通过该 index 读 heap 内对应 descriptor

### Requirement: untyped 不支持 combined image sampler

`DescriptorHeap` 的 untyped 路径 SHALL NOT 支持 `COMBINED_IMAGE_SAMPLER`（combined 跨两个 heap，untyped 无法分别设 image/sampler 的 array stride），SHALL 只允许分离的 sampled image + sampler；typed 路径 SHALL 仍支持 combined。

#### Scenario: untyped 拒绝 combined

- **WHEN** 在 untyped 路径写入 combined image sampler
- **THEN** 报错/拒绝；改用分离的 image + sampler

### Requirement: descriptor heap 能力门

`DeviceFeature` SHALL 记录 `VK_EXT_descriptor_heap` 是否可用（含 `VK_KHR_shader_untyped_pointers` / `SPV_EXT_descriptor_heap` 等依赖）；`CreateDescriptorHeap` 在不可用时 SHALL 返回 nullptr，调用方退化为 tier1。

#### Scenario: 能力上报

- **WHEN** 设备支持 `VK_EXT_descriptor_heap`
- **THEN** 对应 feature 置真，`CreateDescriptorHeap` 可用；否则返回 nullptr
