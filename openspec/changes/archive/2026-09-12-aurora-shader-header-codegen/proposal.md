## Why

shader 与 C++ 之间的资源块布局目前靠人肉保持一致，没有机械约束：C++ 侧 struct（如 `GlobalParams`）是手写的、须与「生成的 cbuffer」手动对齐，而 shader 侧只有测试里的内联字符串。若把 C++ 侧 `RgFieldType` 这种封闭枚举当作事实源，会永远追不上 shader 类型系统的表达能力（嵌套 struct、数组、`uint/int/bool/half`、`RWStructuredBuffer` 等），且仍要手写镜像 struct。**事实源应放在 shader（Slang）侧**：作者只声明一次，编译器反射给出权威的 offset/set/binding，C++ struct 与 `RgBlockDesc` 都从反射派生——漂移在构造上就不可能发生。

## What Changes

- **Slang `.slang` 资产成为单一事实源**：引擎 shader 的资源块（struct + `ParameterBlock` + `[[vk::binding]]`）在 `.slang` 中声明一次。
- **离线反射 codegen**：新增 host 工具（复用 `ShaderCompilerSlang` 的 program layout 反射），产出：
  - C++ 头：镜像 struct（定长、`static_assert` 校验 size/offset）+ 生成的 `RgBlockDesc`（`set/binding/fields/kind`），继续喂 `ResourceGroupLayout`。
  - 可供其它 shader `#include` 的 Slang 共享头。
- **`ShaderCompilerSlang` 扩展**：接入虚拟 include（`ShaderFileSystem`）供 shader 消费共享头；反射结果补充足够类型信息以映射到 C++（Slang 类型 → C++ 类型映射表，有限且稳定）。
- **`RgBlockDesc` 改为反射派生**：不再手写封闭枚举，字段类型由反射反推。
- **首个真实 `.slang` 引擎 shader**：`#include` 生成头，证明机制闭环（替换/补充测试内联字符串）。
- **三方一致性校验**：复用 `ReflectionValidation`，交叉校验生成的 C++ struct、shader 反射、`RgBlockDesc`，纳入测试。
- **不破坏现有**：三层 RG 语义（Global/Pass/Batch）不变；`GlobalRenderResources` 的 `GlobalParams` 与 `GetGlobalBlockDesc()` 改为生成产物。

## Capabilities

### New Capabilities

- `aurora-shader-header-codegen`: 以 Slang shader 为单一事实源，经离线反射生成 shader/C++ 共享头（C++ 镜像 struct + `RgBlockDesc` + 虚拟 include + 类型映射 + 一致性校验）的机制。

### Modified Capabilities

- `aurora-resource-tiers`: `RgBlockDesc 单一事实源` 需求变更——`RgBlockDesc` 由 `.slang` 反射生成（而非手写），codegen 同时产出 C++ 镜像 struct（不再只有 HLSL header）。
- `shader-slang-spike`: `ShaderCompilerSlang` 增加虚拟 include 解析（`ShaderFileSystem`）与 C++ 绑定产物（从反射发射 C++ struct/`RgBlockDesc`）。

## Impact

- **engine/aurora/shader**：`ShaderCompilerSlang` 接入 include 解析与类型映射；新增 `ShaderFileSystem`（virtual include）；新增离线 codegen host 工具入口；`RgBlockDesc` 支持反射反推。
- **engine/aurora/pipeline**：`GlobalRenderResources` 的 `GlobalParams` struct 与 `GetGlobalBlockDesc()` 改由生成头产出（消除手写镜像）。
- **构建**：`Aurora.Shader` / `Aurora.Pipeline` 的 CMake 增加 codegen 自定义命令与生成头依赖（生成物写 build tree）。
- **测试**：新增 codegen 一致性测试（生成头 vs 反射 vs `RgBlockDesc` 三方一致）；`AuroraShaderTest` / `AuroraPipelineTest` 覆盖。
- **范围边界**：不改三层 RG 语义；不做独立 IDL/DSL；不迁移现有 HLSL 资产。
