## MODIFIED Requirements

### Requirement: Device::CreateResourceGroup 是 ResourceGroup 创建的正确入口

`Device` SHALL 提供 `CreateResourceGroup(const ResourceGroup::Descriptor &)` 接口；不再保留 `CreateSampler(const ResourceGroup::Descriptor &)` 这种基于 descriptor 类型的命名重载。

`Device` SHALL 无 `CreatePipelineLayout` 接口（native pipeline layout 由后端在 `Shader` 内派生，见 `aurora-shader-derived-layout`）。

#### Scenario: 调用方按正确名字创建

- **WHEN** 调用 `device->CreateResourceGroup({...})`
- **THEN** 接口存在；返回值符合 ResourceGroup 实际实现状态（本 change 不实现内容；`aurora-resource-group` 中实现）

#### Scenario: 旧名 CreateSampler(ResourceGroup::Descriptor) 不存在

- **WHEN** 调用方尝试 `device->CreateSampler(ResourceGroup::Descriptor{})`
- **THEN** 编译错误（接口已删除该 override）

#### Scenario: CreatePipelineLayout 不存在

- **WHEN** 调用方尝试 `device->CreatePipelineLayout({...})`
- **THEN** 编译错误（接口已删除，布局由 shader 反射派生）
