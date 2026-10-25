## MODIFIED Requirements

### Requirement: UBO 类型按显式绑定声明
Shared declarations SHALL describe the base uniform type. The consuming Pass/material SHALL explicitly select static or dynamic binding and keep shader/layout type, allocation and binding offsets consistent; set numbers SHALL NOT determine this policy.

#### Scenario: Explicit dynamic binding
- **WHEN** a consumer selects a dynamic uniform binding
- **THEN** its shader layout, allocation and supplied offsets agree; a policy mismatch returns an explicit error before drawing

### Requirement: Pass-owned shared header source

每个采用共享声明的 Global/Pass/Material/Batch 接口 SHALL 有一个权威共享 `.h`，直接供 C++ 消费，并由既有工具导出持久、版本化 `.slangh` 供 shader 消费。相同字段/资源列表 SHALL 产生语义接口、typed logical slots 与 CPU schema；CPU sizeof/offsetof SHALL 仅描述 CPU 表示，不要求等于所有目标 ABI。Global/Pass/Material/Batch SHALL 分别使用 logical sets 0/1/2/3，View SHALL 按需归 Pass。

#### Scenario: C++ 无 shader 生成前置
- **WHEN** 编译消费共享声明的 C++ target
- **THEN** 无需执行 Slang、ShaderHeaderTool 或目标平台生成器即可取得 C++ 类型和 CPU schema

#### Scenario: 共享源变更
- **WHEN** 修改权威声明或其导出 helper
- **THEN** 相关 C++、持久 shader export 与 shader 依赖身份按各自输入更新，不生成供 C++ 消费的镜像 ABI header

#### Scenario: 目标布局不同
- **WHEN** 同一平坦 uniform 接口的 CPU 与 SPIR-V、DXIL 或 MSL 目标布局不同
- **THEN** 语义兼容的受支持字段通过 UniformUploadPlan 编码，不因合法 offset/stride 差异而拒绝

### Requirement: 虚拟 include
`ShaderCompilerSlang` SHALL resolve quoted and angle includes through `ShaderFileSystem`, including persisted shader-only .slangh exports and their common helper, without requiring disk copies or native host header generators.

#### Scenario: Packaged scene shaders
- **WHEN** only the built-in scene virtual files are registered, without filesystem search paths
- **THEN** SceneForward, SceneLit, SceneShadow and SceneToneMap compile for each supported target with their shared declaration includes

#### Scenario: Shared helper cache invalidation
- **WHEN** a shared helper reached through an angle include changes
- **THEN** the source closure hash changes and old cache entries are rejected; quoted includes search the including directory before the logical root

### Requirement: 类型映射

共享声明 SHALL 支持 FLOAT/INT/UINT、FLOAT2/FLOAT4、MAT4 和有界 FLOAT4 uniform 数组，保留逻辑字段类型、元素数量及 CPU 表示。目标反射 SHALL 独立描述其实际存储布局和矩阵表示。BOOL/FLOAT3/half、嵌套结构、未支持矩阵或数组、未支持的 storage-buffer 形态与 descriptor arrays SHALL 明确失败，不能遗漏字段。共享声明 SHALL 支持用于 palette 的只读 STRUCTURED/FLOAT4 buffer，并保留访问方式及 element stride。资源-only 声明 SHALL 保留形状及 typed slots，不引入占位 UBO；描述一种资源 SHALL NOT 意味着该执行路径已支持它。

#### Scenario: 有界 uniform 数组
- **WHEN** 共享接口声明 FLOAT4 数组
- **THEN** CPU 与目标各自记录 count/stride，上传计划按元素编码；合法的 stride 差异不要求两侧原始字节相同

#### Scenario: Resource-only 声明
- **WHEN** 接口只包含受支持的 texture/sampler 资源
- **THEN** 产生资源语义与 typed slots，不产生占位常量块

#### Scenario: 不支持字段拒绝
- **WHEN** 声明包含本期不支持的字段类型
- **THEN** 导出或验证明确失败，不能发布缺字段的接口或计划

### Requirement: 三方一致性校验

系统 SHALL 分别验证共享语义、CPU schema 与每个目标 ABI，验证上传后的目标字节与语义值一致。CPU schema SHALL 使用真实 sizeof/offsetof；目标 layout SHALL 来自请求目标自身；RgBlockDesc 等桥接描述 SHALL 继续从同一字段列表派生，不成为独立事实源。合法布局差异 SHALL 由 UniformUploadPlan 处理，语义冲突或不能编码的布局 SHALL 在修改 draw/执行 GPU 前明确失败。各目标编译反射测试 SHALL NOT 代替对应平台原生执行。

#### Scenario: 语义漂移被拒绝
- **WHEN** 目标字段的逻辑类型、数量或含义与共享接口不兼容
- **THEN** 计划构建或装配明确失败，原 draw 不变

#### Scenario: 不同 packing 保持值一致
- **WHEN** CPU schema 与目标 ABI 的 offset、数组 stride 或矩阵表示不同且处于支持集合
- **THEN** 编码后 shader 读到与 CPU 语义相同的值

#### Scenario: 交叉构建
- **WHEN** 移动目标编译共享 C++ 声明
- **THEN** 无需执行目标生成器；交叉构建结果与真机 shader 执行证据分开记录
