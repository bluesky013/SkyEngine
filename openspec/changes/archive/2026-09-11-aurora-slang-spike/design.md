## Context

Slang 调研结论（官方文档核实）：

- **原生 Metal 后端**：`SLANG_METAL` 直出 MSL，不经 SPIRV-Cross；`ParameterBlock` → Metal Argument Buffer；`:register()` 语义保留。
- **AST 内部化**（`slang-ast` 不公开）；**IR 可序列化**（precompiled modules）；反射一等公民（program layout + `IMetaData`）。
- 与本项目的契合点：ParameterBlock ≈ Global/Pass/Batch 三层 RG；generics/link-time specialization 可替代 `#pragma option`；GLES（GLSL ES）与 WGSL 白送。

现有基建：`ShaderCompilerBase` 接口（`CompileBinary`）、DXC/glslang 双编译器、`ShaderReflection` 统一反射结构、`ShaderTool` 离线编译 exe、3rd 包声明在 `cmake/thirdparty.json`。

## Goals / Non-Goals

**Goals:**

- 3rd 引入 slang（预编译二进制，Win32 先行）。
- `ShaderCompilerSlang`：Slang 编译 API 封装，出 SPIRV + MSL 双产物。
- Slang 反射收敛到 `ShaderReflection`。
- spike 测试：ParameterBlock mini shader → 双产物 + 反射断言。

**Non-Goals:**

- 不迁移任何现有 HLSL shader。
- 不动 DXC 主链与 `#pragma option` 体系。
- 不接入 runtime 渲染路径（只编译/反射层验证）。
- 不做 Slang 的 modules/generics 深度使用（spike 只验证 parameter block 映射）。
- MacOS/Metal 实机验证后续（本 spike 只验证 MSL 文本产物正确性）。

## Decisions

### 1. 3rd 包形态：预编译二进制

Slang 官方分发预编译包（slang.zip per platform），比源码构建省事（源码构建要拉一堆子模块）。参照 dxcompiler 的模式声明。Win32 先行；`platforms: ["Win32"]`。

### 2. 编译器封装平行于 DXC

```cpp
class ShaderCompilerSlang : public ShaderCompilerBase {
    // IGlobalSession 单例持有；ISession 按编译配置建
    // loadModuleFromSourceString → compose(module, entryPoint) → link
    // getEntryPointCode(targetIndex) → SPIRV / MSL blob
    // program layout → ShaderReflection
};
```

`ShaderLanguage` 加 `SLANG`；`ShaderCompileTarget` 复用 SPIRV/MSL（DXIL 也可出，spike 不验）。

### 3. 反射收敛

Slang program layout（`slang::ShaderReflection` / VariableLayout）→ 遍历 parameters → 填 `ShaderResource{set, binding, type, name}`。`space` 对应 set。编译后用 `IMetadata::isParameterLocationUsed` 做 dead-strip 校验（替代 Metal 侧 MTLRenderPipelineReflection 的用途）。

### 4. spike 测试 shader

mini fullscreen：3 个 ParameterBlock（Global/Pass/Batch 语义）+ texture 采样 → 断言：
- SPIRV 产物非空、SPIRV-Cross 反射出 3 个 block 的 set/binding 正确；
- MSL 产物文本含 argument buffer 结构（`constant* ... [[buffer(N)]]`）。

## Risks / Trade-offs

- **[Slang 依赖体积]** 预编译包较大（数百 MB 级）。→ spike 期接受；若转正再评估裁剪。
- **[版本绑定]** Slang API 迭代快（COM 接口版本）。→ spike 钉死一个 release tag。
- **[MSL 文本无实机验证]** 本 spike 在 Win32 只验证文本结构。→ Metal 实机编译验证留 MacOS 环境。

## Migration Plan

1. 3rd：thirdparty.json 加 slang 包 + FindSlang.cmake（预编译包引入）+ bootstrap 拉取。
2. `ShaderCompilerSlang` 实现（SPIRV + MSL + 反射）。
3. spike 测试（ShaderCompilerTest 内新增）。
4. 结果评估 + archive（迁移决策另起 change）。
