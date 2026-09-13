# aurora-resource-binding Specification Delta

## MODIFIED Requirements

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
