## Why

Slang 调研（官方文档已核实）确认两个关键能力：

1. **Slang 原生直出 Metal**（`-target metal` / `SLANG_METAL`），不经 SPIRV-Cross——`ParameterBlock` 直接翻译成 Metal Argument Buffer，与我们 Global/Pass/Batch 三层 RG 天然同构；`:register()` 语义在 Metal 目标下被尊重。
2. **AST 在内部**（`slang-ast` 模块不公开），但 **Slang IR 可序列化落盘**（precompiled modules），公开 API 层面 IR 是可复用的中间产物；反射为一等公民（program layout + 编译后 `IMetaData::isParameterLocationUsed`）。

引入 Slang 的收益：Metal 无中转、parameter block 语言级表达三层布局、原生特化替代 `#pragma option`、GLES/WebGPU 白送。代价：3rd 依赖体积、16 个 HLSL shader 迁移、布局体系重映射。**先 spike 最小验证，不动主链。**

## What Changes

- **3rd**：`cmake/thirdparty.json` 新增 `slang` 包（预编译二进制分发；Win32 先行，MacOS 后续）。
- **新增** `ShaderCompilerSlang`（`engine/shader`，与 `ShaderCompilerDXC`/`ShaderCompilerGlsl` 平行的 `ShaderCompilerBase` 实现）：
  - Slang Compilation API（`IGlobalSession`/`ISession`/`loadModuleFromSourceString`/`getEntryPointCode`）
  - 目标：SPIRV（VK）+ MSL（Metal）双产物
  - 反射：Slang 自家 reflection API（program layout）→ 收敛到 `ShaderReflection`（`ShaderResource{set,binding,type,...}`）
- **spike 验证**：一个带 ParameterBlock 的 mini fullscreen shader，Slang 编译出 SPIRV + MSL，反射结果与预期 set/binding 比对。
- **不动**：DXC 主链、现有 16 个 HLSL shader、`#pragma option` 体系、GLES。

## Capabilities

### New Capabilities

- `shader-slang-spike`: Slang 编译通道的 spike 验证契约（双产物、反射收敛、parameter block 映射）。

### Modified Capabilities

无（纯新增，不动现有能力）。

## Impact

- **3rd**：`cmake/thirdparty.json` + slang 包。
- **engine/shader**：新增 `ShaderCompilerSlang.h/.cpp`；`ShaderCompiler` 注册新语言/目标入口。
- **测试**：`ShaderCompilerTest` 或独立 spike 测试（Slang SPIRV/MSL 编译 + 反射断言）。
- **范围边界**：spike 只验证可行性；迁移决策（是否切主链/迁移 shader 资产）在 spike 结果出来后再定。
