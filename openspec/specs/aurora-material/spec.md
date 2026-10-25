# aurora-material Specification

## Purpose
TBD - created by archiving change aurora-material. Update Purpose after archive.
## Requirements
### Requirement: Material 内部按 technique 维护

Material SHALL own tagged MaterialTechnique records containing an explicit ShaderTechnique program (stage/source/entry), checked legacy aliases and material render state. Scene queues SHALL select matching tags; fullscreen and compute SHALL select programs explicitly without requiring Material. Empty or duplicate Material technique tags SHALL fail. MaterialRenderState SHALL contain depth/stencil, raster, blend and alpha-to-coverage state, excluding mesh vertex layout/topology and Pass sample count. Shader resolution SHALL retain the selected program rather than substitute an implicit built-in shader.

#### Scenario: Tagged stage programs
- **WHEN** opaque and shadow techniques reference different explicit stage programs
- **THEN** each queue uses its matching program and material state

#### Scenario: Invalid tag
- **WHEN** a technique has an empty or duplicate tag
- **THEN** adding it fails without changing the existing techniques

### Requirement: Material 属性系统（material 级，值 + 纹理）

Material SHALL 提供跨 technique 共享的 Name 属性，属性类型继续复用 RgFieldType 的词汇，不建立另一套材质字段类型枚举。CPU 值按其声明语义存储，字节大小由类型派生；MaterialPropertyMap SHALL 只描述 CPU 属性位置，不表示 GPU offset。SetValue/GetValue SHALL 校验 T 的语义类型/shape，SHALL NOT 仅凭 sizeof 通过校验；未知名或不匹配 SHALL 返回明确结果且不写入。

纹理 SHALL 按 Name 持有合法的 Texture 引用/逻辑资源身份，sampler SHALL 使用独立的类型化设置/引用，不能作为 Texture 伪装。新资产的参数 Key SHALL 来自完整 Material group 声明、唯一 cbuffer 字段与 Shader-local 选项，按 canonical Name 映射；显式 aliases SHALL 仅作为旧数据的受检迁移输入，最终进入ResourceGroup 共用的同一个 RHI 命名事务前端及布局计划，不暴露平台 slot/offset。CPU 属性可保留既有类型词汇，但执行路径不支持的字段/资源 SHALL 在受检绑定前明确 Unsupported；本次不自动扩大 shader 字段支持集合。

#### Scenario: 按类型声明并读写
- **WHEN** 声明 FLOAT4 的 baseColor 并按 Name 写入 Vector4
- **THEN** 可读回对应 CPU 值，GetPropertyMap 保留该 CPU 属性的类型与位置

#### Scenario: 相同大小不能替代类型
- **WHEN** 对 FLOAT 属性按 Name 写入 uint32_t
- **THEN** 明确类型错误且原值不变，即使二者同为四字节

#### Scenario: Size 由 type 派生
- **WHEN** 读取 material CPU 属性元数据
- **THEN** 值大小由声明语义类型派生，不重复存储另一份可漂移 size，也不当作 GPU block offset

#### Scenario: 类型词汇共用
- **WHEN** material 描述已有 scalar/vector/matrix 或资源属性
- **THEN** 复用共享字段词汇，执行能力由实际支持表校验

#### Scenario: 纹理与 sampler 挂接
- **WHEN** 按 Name 分别设置合法纹理与 sampler
- **THEN** 各自进入匹配逻辑资源，类型不兼容时明确失败，sampler 不以 Texture 容器表示

### Requirement: MaterialInstance 部分字段 runtime 稀疏覆盖

MaterialInstance SHALL 持有共享 Material 与实例覆盖。值覆盖 SHALL 延续单个连续 CPU 存储、首次写入惰性复制的表示，不逐字段分配容器；纹理/资源覆盖 SHALL 保持稀疏表示。Name 查找与语义类型校验 SHALL 与 Material 相同，失败返回明确结果且保留旧值。未覆盖项 SHALL 继承 Material 默认值，实例更新 SHALL 不污染共享 Material。

SetMaterial SHALL 清空该实例旧覆盖；IsOverridden SHALL 表达显式覆盖是否存在，包括显式写入与默认值相同的值。未设置 Material 的写入 SHALL 返回明确错误而非静默成功，读取返回缺失。实例最终参数 SHALL 按 technique/interface 的 canonical Name 进入与 ResourceGroup 共用的 RHI 更新事务前端，并冻结对应值/资源版本供帧使用。

#### Scenario: 部分覆盖与继承
- **WHEN** 实例只按 Name 覆盖 baseColor
- **THEN** baseColor 使用实例值，其它属性继承默认值，覆盖状态可查询

#### Scenario: 覆盖不污染共享 Material
- **WHEN** 实例更新值、纹理或 sampler
- **THEN** 共享 Material 及其它实例保持原值

#### Scenario: 值覆盖保持连续存储
- **WHEN** 一个实例只覆盖一个 scalar 字段
- **THEN** 值仍存于实例的单块连续存储，不逐字段创建独立容器

#### Scenario: 未设置 Material
- **WHEN** 未关联 Material 的实例执行写入或读取
- **THEN** 写入返回明确错误，读取返回缺失，不崩溃也不伪装成功

#### Scenario: 更新与在途帧隔离
- **WHEN** 旧帧未退休时实例替换贴图或改变数值
- **THEN** 新帧使用新快照，旧帧继续读取旧值与精确图片版本

### Requirement: Generated Material parameter keys

Material logical keys SHALL be generated from the stable Group 2 declaration union of its techniques and Shader-local option schemas. Keys SHALL distinguish fields, textures, samplers and options, retain semantic types and avoid persisting target byte offsets as authoring schema. Cross-technique shared keys SHALL agree on semantics/type/shape. Display names SHALL NOT create implicit aliases. Existing aliases SHALL migrate values explicitly and incompatible mappings SHALL fail diagnostically.

#### Scenario: Technique subset
- **WHEN** one technique uses only part of the Material's logical key table
- **THEN** other known keys remain stored without requiring active bindings for that technique

### Requirement: One logical Material cbuffer

A linked program family SHALL declare zero or one logical Group 2 cbuffer after merging stages. Repeated references to the same block SHALL count once; distinct blocks SHALL fail even if only one is active in a particular choice. Target stage ABI for the shared block SHALL be compatible. Pass/Batch groups SHALL retain their own resource contracts.

#### Scenario: Two material blocks
- **WHEN** Group 2 declares two distinct buffers
- **THEN** schema validation rejects the program before publication

### Requirement: Inactive Material values remain valid

Known inactive keys SHALL retain values and skip only current target writes. Unknown keys, semantic type errors and missing required active values/resources SHALL fail. Required resources SHALL be determined by the actual target contract, not inferred from a false option. Texture residency SHALL NOT implicitly change static options. MaterialInstance SHALL keep value/texture/sampler overrides without new static-option overrides.

#### Scenario: Disabled texture option
- **WHEN** a texture option is false but the native artifact still requires the texture
- **THEN** binding supplies a valid resource or explicit default, or reports missing-resource

#### Scenario: Inactive wrong type
- **WHEN** an inactive known field receives the wrong semantic type
- **THEN** validation still rejects it
