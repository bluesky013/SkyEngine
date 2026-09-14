# aurora-resource-binding Specification Delta

## MODIFIED Requirements

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
