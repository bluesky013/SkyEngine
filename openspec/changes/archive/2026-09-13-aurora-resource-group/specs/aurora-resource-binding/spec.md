## ADDED Requirements

### Requirement: ResourceGroupLayout 描述一组绑定槽

`Device::CreateResourceGroupLayout(const ResourceGroupLayout::Descriptor &)` SHALL 返回一个不可变的 `ResourceGroupLayout` 实例。`Descriptor::bindings` 是一组 `BindingDesc`：

- `uint32_t binding`：set 内 binding 索引；同一 layout 内必须唯一
- `DescriptorType type`：复用现有 `DescriptorType` 枚举（SAMPLER / COMBINED_IMAGE_SAMPLER / SAMPLED_IMAGE / STORAGE_IMAGE / UNIFORM_BUFFER / STORAGE_BUFFER / UNIFORM_BUFFER_DYNAMIC / STORAGE_BUFFER_DYNAMIC / INPUT_ATTACHMENT）
- `uint32_t count`：array size，必须 ≥ 1；本 change 不支持 VARIABLE_COUNT
- `ShaderStageFlags stages`：可见 stage
- `DescriptorBindingFlags flags`：本 change 内 MUST 不含 `VARIABLE_COUNT`

#### Scenario: 创建含 uniform + sampled image 的 layout

- **WHEN** 调用方传入 bindings = [{binding=0, type=UNIFORM_BUFFER, count=1, stages=GFX}, {binding=1, type=COMBINED_IMAGE_SAMPLER, count=1, stages=FS}]
- **THEN** 返回非空 `ResourceGroupLayout*`

#### Scenario: 重复 binding 报错

- **WHEN** 调用方传入两个 binding 索引相同的 BindingDesc
- **THEN** 返回 nullptr，且 logger 报错

#### Scenario: VARIABLE_COUNT 标记被拒绝

- **WHEN** 调用方某个 BindingDesc.flags 含 `VARIABLE_COUNT`
- **THEN** 返回 nullptr（本 change 范围外）

### Requirement: 跨后端 binding 索引一致性约定

Aurora SHALL 保证：在 layout 中声明的 (set, binding) 索引在所有后端表现一致：

- Vulkan：直接 `layout(set=N, binding=M)`
- DX12：通过 root signature 的 register space + register 映射（space=N, register=M）
- Metal：argument buffer slot=N，buffer/texture index 由 layout 表内部分配

调用方写一份 layout，多后端 binding 行为一致；shader 编译产物可不同（每后端单独编译），但 layout 描述统一。

#### Scenario: 同一 layout 跨后端

- **WHEN** 用同一 layout（set 0：binding 0=UB, binding 1=COMBINED）+ 各自后端编译的 shader 创建 pipeline
- **THEN** 各后端中 BindResourceGroup + Draw 行为一致（采到同一纹理、读到同一 UB 数据）

### Requirement: Device::CreateResourceGroup 创建 group 实例

`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 按 `Descriptor::layout` 分配一个新的 ResourceGroup 实例。

`layout` MUST 非空；返回的 ResourceGroup 在创建后所有 binding 均为"未写入"状态，必须先 `Update` 才能在 BindResourceGroup 后被采样。

#### Scenario: 创建空 group

- **WHEN** 调用 `device->CreateResourceGroup({.layout = layout})`
- **THEN** 返回非空 `ResourceGroup*`

#### Scenario: layout 为空拒绝

- **WHEN** `Descriptor::layout = nullptr`
- **THEN** 返回 nullptr

### Requirement: ResourceGroup::Update 批量写入资源

`ResourceGroup::Update(const std::vector<ResourceUpdateInfo> &writes)` SHALL 把 writes 中的资源指针按 (binding, arrayElement) 写入 group。

`ResourceUpdateInfo` 含：
- `uint32_t binding`、`uint32_t arrayElement`
- `ResourceWriteKind kind`（BUFFER / IMAGE / SAMPLER / COMBINED_IMAGE_SAMPLER）
- 联合体携带 Buffer*/offset/range 或 Image*/layout 或 Sampler* 或 (Image*/Sampler*/layout)

写入的 (binding, kind) 组合 MUST 与 layout 中该 binding 的 `DescriptorType` 兼容；不兼容时 debug build assert，release build 行为未定义。

`Update` 可被多次调用；新写入覆盖旧写入；未被覆盖的 binding 保留先前值。

#### Scenario: 写入 uniform + sampled image

- **WHEN** 对 layout=[binding 0=UB, binding 1=COMBINED] 的 group 调用 Update 写入 [{binding=0, BUFFER, buf=ubuf, offset=0, range=64}, {binding=1, COMBINED_IMAGE_SAMPLER, image=tex, sampler=smp, layout=SHADER_READ_ONLY}]
- **THEN** Update 不报错；后续 BindResourceGroup + Draw 可正确采样到 tex 与 ubuf 数据

#### Scenario: 写入类型不匹配 binding type

- **WHEN** 对 binding=0（UNIFORM_BUFFER）写入 IMAGE
- **THEN** Debug build assert；Release build 行为未定义但 MUST 不静默成功

### Requirement: Encoder::BindResourceGroup 真正生效，支持动态偏移

`GraphicsEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)` 与 `ComputeEncoder::BindResourceGroup(...)` SHALL 把 group 绑定到当前 pipeline 的对应 set 槽。

`set` SHALL 对应当前 pipeline 经 shader 反射派生出的某个 descriptor set。

`numDynamicOffsets` MUST 等于该 group 的 layout 中 `*_DYNAMIC` 类型 binding 的总数；offsets 顺序与 binding 索引顺序一致。

#### Scenario: 单 set 绑定后 Draw

- **WHEN** BindPipeline + BindResourceGroup(0, group, 0, nullptr) + Draw
- **THEN** Draw 不报错；validation layer 不报"missing descriptor set"

#### Scenario: 动态偏移

- **WHEN** layout binding 0 是 UNIFORM_BUFFER_DYNAMIC，调用 BindResourceGroup(0, group, 1, &offset0)
- **THEN** shader 读取该 UB 时基地址加上 offset0；可用两次同 group 不同 offset 验证

#### Scenario: 多 set

- **WHEN** 绑定 set 0 + set 1，BindResourceGroup(0, g0) + BindResourceGroup(1, g1) + Draw
- **THEN** 两个 set 都生效；shader 内同时读 set=0 与 set=1 的 binding

### Requirement: Encoder::PushConstants 写入 push constants

`GraphicsEncoder::PushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data)` 与 `ComputeEncoder::PushConstants(uint32_t offset, uint32_t size, const void *data)` SHALL 把 `data` 写入当前 pipeline 的 push constants 区域。

`(offset, size)` MUST 落在 shader 反射声明的 push constant 区间内。`stages` MUST 是该区间 stage 的子集（VK 校验规则）。

#### Scenario: 写入 push constants

- **WHEN** shader 声明 push constant 区间 offset=0/size=64，调用 `PushConstants(GFX, 0, 16, data)` 后 Draw
- **THEN** shader 中 push constants 前 16 字节是 data 内容（其余字节未定义）

#### Scenario: 越界 offset+size 拒绝

- **WHEN** shader 区间是 [0, 64)，调用 `PushConstants(GFX, 60, 16, ...)`
- **THEN** Debug assert；Release 行为未定义

### Requirement: Array binding 支持 count > 1

ResourceGroupLayout 中 `count > 1` 的 binding（如纹理数组）MUST 支持通过 `Update` 按 `arrayElement` 索引写入；BindResourceGroup 后 shader 可以索引 `arr[i]`。

#### Scenario: 8 元素 sampled image 数组

- **WHEN** layout binding=0、type=COMBINED_IMAGE_SAMPLER、count=8；Update 8 次（arrayElement 0..7）；Draw shader 读 arr[3]
- **THEN** 采样到 arrayElement=3 写入的纹理
