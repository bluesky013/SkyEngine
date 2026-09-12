## ADDED Requirements

### Requirement: 反射驱动 codegen

离线 codegen（host 工具）SHALL 以 `.slang` shader 反射为输入，对每个 resource block 产出两侧共享头：C++ 镜像 struct 与 `RgBlockDesc`。

生成的 C++ struct SHALL 逐字段使用 `alignas` 对齐 std140 布局，并携带 `static_assert(sizeof)` 与逐字段 `static_assert(offsetof)`；offset/size SHALL 来自 slang 反射，而非 C++ 侧重算。

#### Scenario: 生成 C++ 镜像 struct

- **WHEN** 对声明 `struct GlobalParams { float4x4 view; float4x4 proj; float4x4 viewProj; float4 cameraPos; }` 的 `.slang` 做 codegen
- **THEN** 产出含 `Matrix4 view/proj/viewProj` 与 `Vector4 cameraPos` 的 C++ struct，且 `static_assert(offsetof(cameraPos) == 64)` 与 `static_assert(sizeof == 128)` 成立

#### Scenario: 生成 RgBlockDesc

- **WHEN** 对含 `[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal` 的 `.slang` 做 codegen
- **THEN** 产出 `GetGlobalBlockDesc()` 返回 `RgBlockDesc{set=0, binding=0, blockName="Global", kind=CBUFFER}`，字段顺序与 struct 成员一致

### Requirement: 虚拟 include

`ShaderCompilerSlang` SHALL 支持虚拟 include：经 `ShaderFileSystem`（Slang 文件系统回调）解析 shader 中的 `#include`，命中内存中生成的共享头，无需落盘。

#### Scenario: include 生成头

- **WHEN** shader 源码含 `#include "Generated/GlobalBlock.slang"` 且该路径在 `ShaderFileSystem` 中有对应内存内容
- **THEN** 编译成功；该路径无需存在于磁盘

### Requirement: 类型映射

`ShaderTypeMap` SHALL 把反射得到的 slang 类型映射到 C++ 类型（`float4x4`→`Matrix4`、`float4`→`Vector4`、`uint`→`uint32_t`、`bool`→`uint32_t` 等），定长数组与嵌套 struct 递归映射。

未支持的 slang 类型 SHALL 使 codegen 失败并报错，而非静默降级。

#### Scenario: 标量/向量/矩阵映射

- **WHEN** 反射类型为 `float4x4`
- **THEN** 生成 C++ 类型 `Matrix4`

#### Scenario: 未支持类型报错

- **WHEN** 反射到映射表未覆盖的 slang 类型
- **THEN** codegen 返回错误，不产出缺字段的 struct

### Requirement: 三方一致性校验

生成的 C++ struct 与 `RgBlockDesc` SHALL 与 shader 反射保持一致：编译期由 `static_assert` 把关布局，运行时由 `ReflectionValidation` 交叉比对 `RgBlockDesc` 与运行时 shader 反射（set/binding/字段顺序/类型/offset）。

#### Scenario: 布局漂移编译失败

- **WHEN** 生成的 struct 尺寸/offset 与反射不一致
- **THEN** `static_assert` 触发，编译失败

#### Scenario: 运行时校验

- **WHEN** 运行时 `ValidateBlockAgainstReflection(desc, reflection)`
- **THEN** 一致时返回空串；不一致时返回描述差异的错误信息
