## MODIFIED Requirements

### Requirement: Slang 编译通道

`ShaderCompilerSlang` SHALL 经 Slang Compilation API（`IGlobalSession` / `ISession` / `loadModuleFromSourceString` / compose+link / `getEntryPointCode`）把 Slang 源码编译为 SPIRV 与 MSL 双产物。

Slang 程序布局（program layout）SHALL 收敛到统一 `ShaderReflection`（`ShaderResource{set, binding, type, name}`）；`space` 映射 set。

`ShaderCompilerSlang` SHALL 支持虚拟 include：经 `ShaderFileSystem` 解析 shader 中的 `#include`，命中内存中生成的共享头，无需落盘。

#### Scenario: SPIRV 产物

- **WHEN** 编译含 ParameterBlock 的 mini shader 目标 SPIRV
- **THEN** 产物非空；SPIRV-Cross 反射出各 block 的 set/binding 与源码声明一致

#### Scenario: MSL 产物直出

- **WHEN** 同一 shader 目标 MSL
- **THEN** 产物为 MSL 文本，不经 SPIRV-Cross；ParameterBlock 翻译为 argument buffer 形态（`constant* ... [[buffer(N)]]`）

#### Scenario: 反射收敛

- **WHEN** Slang 编译完成
- **THEN** `ShaderBuildResult.reflection.resources` 含各资源的 set/binding/type/name，结构与 DXC 通道同构

#### Scenario: 虚拟 include

- **WHEN** shader 源码含 `#include "Generated/GlobalBlock.slang"`
- **THEN** 编译器经 `ShaderFileSystem` 解析该路径为内存内容并成功编译；该路径不存在于磁盘
