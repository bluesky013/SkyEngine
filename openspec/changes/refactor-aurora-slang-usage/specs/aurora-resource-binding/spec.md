## ADDED Requirements

### Requirement: Cbuffer 使用 per-target 布局与上传计划

Cbuffer（uniform buffer）SHALL 允许每个 target 使用 Slang 原生 offset/size/alignment/stride。CPU 侧 SHALL 使用 canonical `SharedShaderTypes` schema，并通过 `UniformUploadPlan` 按字段名映射到目标布局；SHALL NOT 要求 CPU offset/alignment 等于目标值，也 SHALL NOT 为了测试或上传方便强制所有 target 使用同一 canonical layout。本 change SHALL NOT 重定义 `refactor-aurora-target-shader-artifacts` 的 `UniformUploadPlan`，只规定 cbuffer/SSBO 的适用边界。

`RuntimeSpecialized` variant 的最终表达由后端决定：SPIRV/Metal 使用 specialization constant；D3D12 使用 root constants。不得引入 variant cbuffer fallback，调用方不得选择实现。

`ReflectBlock` SHALL 保留目标原生 layout，不覆盖 member/block alignment。共享头校验 SHALL 使用 `BuildUniformUploadPlan` 验证兼容性，而不是比较 member 全等。

#### Scenario: DXIL 与 SPIRV 布局不同

- **WHEN** 同一 cbuffer 在 DXIL 与 SPIRV 下产生不同的 float4 alignment/offset
- **THEN** 两个 target 都保留原生反射；CPU canonical schema 可分别构建成功的 `UniformUploadPlan`

#### Scenario: 共享头兼容

- **WHEN** 共享 `.h` 的 CPU schema 与某 target 的 cbuffer 原生布局字段类型/数量一致，但 offset/alignment 不同
- **THEN** `BuildUniformUploadPlan` SHALL 成功，不要求 layout 全等

#### Scenario: 不强制 canonical

- **WHEN** 检查 shader 编译结果
- **THEN** 不得存在把 DXIL alignment 改成 SPIRV/C++ alignment 的 canonical 覆盖

#### Scenario: D3D12 specialized variant

- **WHEN** 后端为 D3D12，且 variant 为 `RuntimeSpecialized`
- **THEN** 值 SHALL 通过 root constants 绑定；调用方接口不变；root signature DWORD 数量 SHALL 在编译/cook 期校验

### Requirement: SSBO 使用 canonical 布局并精确校验

SSBO（structured/raw storage buffer）的 CPU 字节 SHALL 与 GPU 读取布局一致。Aurora SHALL 为 SSBO 定义单一 canonical layout，并在编译/cook 阶段按 target 精确校验；SHALL NOT 为 SSBO 生成 per-target 上传转换。

约束：

- `StructuredBuffer<T>` / `RWStructuredBuffer<T>` SHALL 使用默认 layout + 强制 16 字节字段/padding；`Std430DataLayout`/`CDataLayout` 在 Metal/DXIL 不可用，不得依赖；
- SHALL NOT 使用 `float3`；使用 `float4` + 显式 padding；
- 使用固定宽度整数；
- 矩阵 SHALL 显式 `row_major` / `column_major`；
- 数组 SHALL 使用显式/可验证 stride；
- `ByteAddressBuffer` / `RWByteAddressBuffer` SHALL 使用共享 layout 常量或头文件定义 offset，不依赖反射推断；
- 全局 `VulkanUseGLLayout` / `ForceScalarLayout` 等选项不得被用来隐式改变 SSBO 布局而影响 cbuffer。

SSBO 的 CPU canonical schema SHALL 来自同一权威声明/layout 定义，不得默认复用 cbuffer 的 `SharedShaderTypes` schema。

反射 SHALL 记录 storage buffer 的元素 layout：size、stride、alignment、字段 offset/size/type、数组 stride、矩阵 stride/major。`ValidateStorageLayout` SHALL 要求 CPU schema 与每个 target 的 native layout 完全一致；不一致时编译/cook 明确失败。

#### Scenario: 三 target 布局一致

- **WHEN** 同一 16 字节 canonical `RWStructuredBuffer<T>` 编译到 SPIRV、MSL、DXIL
- **THEN** 反射的 element size/stride/field offset/matrix layout SHALL 与 CPU canonical schema 完全一致

#### Scenario: 不一致拒绝

- **WHEN** 某 target 的 SSBO 字段 offset/stride 与 CPU schema 不同
- **THEN** 编译或 cook SHALL 返回显式错误，不生成运行期转换或静默降级

#### Scenario: CPU/GPU 往返

- **WHEN** CPU 按 canonical schema 写入 SSBO，GPU 读取并写回
- **THEN** CPU 读回值 SHALL 与写入/预期一致

#### Scenario: ByteAddressBuffer

- **WHEN** 使用 `RWByteAddressBuffer` 手动 offset
- **THEN** offset 常量 SHALL 来自共享 layout 定义，并有三 target 一致性测试
