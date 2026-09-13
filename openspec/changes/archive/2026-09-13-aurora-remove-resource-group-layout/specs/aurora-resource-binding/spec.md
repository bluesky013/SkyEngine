## REMOVED Requirements

### Requirement: ResourceGroupLayout 描述一组绑定槽

**Reason**: `ResourceGroupLayout` 与 shader reflection 承载相同的 binding 信息（binding/type/count/stages），形成两套 layout，一致性只能靠 codegen 隐式保证。移除后 ResourceGroup 直接从 shader reflection 派生布局（见 `aurora-shader-derived-resource-group`）。

**Migration**: 删除 `Device::CreateResourceGroupLayout` 与 `ResourceGroupLayout` 类型；ResourceGroup 创建改用 `ResourceGroup::Descriptor{shader, set}`。上层 `RgBlockDesc::ToLayoutDescriptor` 路径删除。

## MODIFIED Requirements

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

`ResourceGroup::Update(const std::vector<ResourceUpdateInfo> &writes)` SHALL 把 writes 中的资源指针按 (binding, arrayElement) 写入 group。

`ResourceUpdateInfo` 含：
- `uint32_t binding`、`uint32_t arrayElement`
- `ResourceWriteKind kind`（BUFFER / IMAGE / SAMPLER）
- 联合体携带 Buffer*/offset/range 或 Image*/layout 或 Sampler*

写入的 (binding, kind) 组合 MUST 与 shader reflection 中该 binding 的 `ShaderResourceType` 兼容；不兼容时 debug build assert，release build 行为未定义。

`Update` 可被多次调用；新写入覆盖旧写入；未被覆盖的 binding 保留先前值。

#### Scenario: 写入 uniform + sampled image

- **WHEN** 对 set 含 binding 0=uniform buffer、binding 1=sampled image、binding 2=sampler 的 group 调用 Update 写入 [{binding=0, BUFFER, buf=ubuf, offset=0, range=64}, {binding=1, IMAGE, image=tex}, {binding=2, SAMPLER, sampler=smp}]
- **THEN** Update 不报错；后续 BindResourceGroup + Draw 可正确采样到 tex 与 ubuf 数据

#### Scenario: 写入类型不匹配 binding type

- **WHEN** 对 binding=0（uniform buffer）写入 IMAGE
- **THEN** Debug build assert；Release build 行为未定义但 MUST 不静默成功
