# aurora-resource-binding Specification

## Purpose
TBD - created by archiving change aurora-resource-group. Update Purpose after archive.
## Requirements
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

`Device::CreateResourceGroup(const ResourceGroup::Descriptor &)` SHALL 按 `Descriptor::{shader, set}` 分配一个新的 ResourceGroup 实例。

`shader` MUST 非空；`set` SHALL 是 shader reflection 中存在的 descriptor set。返回的 ResourceGroup 在创建后所有 binding 均为"未写入"状态，必须先 `Update` 才能在 BindResourceGroup 后被采样。ResourceGroup 的 binding 布局 SHALL 等于 shader reflection 中该 set 的完整资源集。

#### Scenario: 创建 group

- **WHEN** 调用 `device->CreateResourceGroup({.shader = shader, .set = 0})`
- **THEN** 返回非空 `ResourceGroup*`

#### Scenario: shader 为空拒绝

- **WHEN** `Descriptor::shader = nullptr`
- **THEN** 返回 nullptr

### Requirement: ResourceGroup::Update 批量写入资源

`ResourceGroup` SHALL 通过 `CreateEncoder()` 返回的 `DescriptorEncoder` 写入资源：调用 `WriteBuffer` / `WriteImage` / `WriteSampler` 按 (binding, arrayElement) 积累写入，`End()` 一次性提交到 descriptor set。

写入的 (binding, kind) 组合 MUST 与 shader reflection 中该 binding 的 `ShaderResourceType` 兼容；不兼容时 debug build assert，release build 行为未定义。

`End()` 可被多次调用（每次一组新写入）；新写入覆盖旧写入；未被覆盖的 binding 保留先前值。

#### Scenario: 写入 uniform + sampled image

- **WHEN** 对 set 含 binding 0=uniform buffer、binding 1=sampled image、binding 2=sampler 的 group，用 encoder `WriteBuffer(0, ubuf, 0, 64)` + `WriteImage(1, tex, SHADER_READ_ONLY)` + `WriteSampler(2, smp)` 后 `End()`
- **THEN** `End()` 不报错；后续 BindResourceGroup + Draw 可正确采样到 tex 与 ubuf 数据

#### Scenario: 写入类型不匹配 binding type

- **WHEN** 对 binding=0（uniform buffer）调用 `WriteImage`
- **THEN** Debug build assert；Release build 行为未定义但 MUST 不静默成功

### Requirement: Encoder::BindResourceGroup 真正生效，支持动态偏移

`GraphicsEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)` 与 `ComputeEncoder::BindResourceGroup(...)` SHALL 把 group 绑定到当前 pipeline 的对应 set 槽。

`set` SHALL 对应当前 pipeline 经 shader 反射派生出的某个 descriptor set。

`BindResourceGroup` SHALL 绑定该 group 的**当前帧 set 视图**：transient 模式（Global/Pass）下是本次更新刚申请的 set；cached 模式（Batch）下是持久 set。绑定前 SHALL 完成该 group 的写入提交（Vulkan `vkUpdateDescriptorSetWithTemplate` / `vkUpdateDescriptorSets`；DX12 的 staging→shader-visible copy）。

`numDynamicOffsets` MUST 等于该 group 的 layout 中 `*_DYNAMIC` 类型 binding 的总数；offsets 顺序与 binding 索引顺序一致。

#### Scenario: 绑定当前帧的 set 视图

- **WHEN** transient 模式下帧 N 更新 Global set 后 `BindResourceGroup(0, group)`
- **THEN** 绑定的是本次更新刚申请的新 set（不是上一帧的旧 set）

#### Scenario: 动态偏移

- **WHEN** layout binding 0 是 UNIFORM_BUFFER_DYNAMIC，调用 BindResourceGroup(0, group, 1, &offset0)
- **THEN** shader 读取该 UB 时基地址加上 offset0

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

