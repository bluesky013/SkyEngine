# shader-slang-spike Specification

## Purpose
TBD - created by archiving change aurora-slang-spike. Update Purpose after archive.
## Requirements
### Requirement: Slang 编译通道

`ShaderCompilerSlang` SHALL 实现 `ShaderCompilerBase`，经 Slang Compilation API（`IGlobalSession` / `ISession` / `loadModuleFromSourceString` / compose+link / `getEntryPointCode`）把 Slang 源码编译为 SPIRV 与 MSL 双产物。

Slang 程序布局（program layout）SHALL 收敛到统一 `ShaderReflection`（`ShaderResource{set, binding, type, name}`）；`space` 映射 set。

#### Scenario: SPIRV 产物
- **WHEN** 编译含 ParameterBlock 的 mini shader 目标 SPIRV
- **THEN** 产物非空；SPIRV-Cross 反射出各 block 的 set/binding 与源码声明一致

#### Scenario: MSL 产物直出
- **WHEN** 同一 shader 目标 MSL
- **THEN** 产物为 MSL 文本，不经 SPIRV-Cross；ParameterBlock 翻译为 argument buffer 形态（`constant* ... [[buffer(N)]]`）

#### Scenario: 反射收敛
- **WHEN** Slang 编译完成
- **THEN** `ShaderBuildResult.reflection.resources` 含各资源的 set/binding/type/name，结构与 DXC 通道同构

