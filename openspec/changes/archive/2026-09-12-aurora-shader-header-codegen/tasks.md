## 1. 虚拟 include（ShaderFileSystem）

- [x] 1.1 新增 `engine/aurora/shader/include/aurora/shader/ShaderFileSystem.h`：定义虚拟 include 接口（`AddVirtualFile(path, content)` / `Find(path)`）
- [x] 1.2 实现 Slang 文件系统回调适配（`ISlangFileSystem`：`loadFile` → `slang_createBlob`），使 `#include` 命中内存内容
- [x] 1.3 `ShaderCompilerSlang::Compile` 接入 include 解析回调；`ShaderCompileDesc` 增加可选 `fileSystem` 参数
- [x] 1.4 测试：shader 源码含 `#include "Generated/GlobalBlock.slang"`（仅存在于内存）能成功编译出 SPIRV

## 2. 类型映射与 codegen 核心

- [x] 2.1 新增类型映射（`ShaderCodeGen::MapField`）：slang 反射 kind/scalarType/rows/cols → C++ 类型（float4x4→Matrix4、float4→Vector4、uint→uint32_t、bool→uint32_t）
- [x] 2.2 扩展 `RgFieldType`（新增 INT/UINT/BOOL），反射 `ShaderBlockMember` 携带 scalarType/kind/rows/cols
- [x] 2.3 实现反射 → C++ struct 发射器：逐字段 `alignas` 对齐 + `static_assert(sizeof/offsetof)`（offset/size 来自 slang 反射）
- [x] 2.4 实现反射 → `RgBlockDesc` 发射器：从反射读出 set/binding/kind/fields，生成 `GetXxxBlockDesc()`
- [x] 2.5 未支持类型使 codegen 报错（非静默降级）

## 3. 离线 codegen 工具与构建集成

- [x] 3.1 新增 host 工具 `ShaderHeaderTool`（链接 `Aurora.Shader` + slang），输入 `.slang` → 输出 `*.gen.h`
- [x] 3.2 `Aurora.Shader` CMake：`add_custom_command` 跑 codegen，生成物写 build tree，`OUTPUT`/`DEPENDS` 增量化
- [x] 3.3 `Aurora.Pipeline` CMake：把 gen 头目录加入 private include path + `add_dependencies(ShaderHeaders)`
- [x] 3.4 验证：`cmake --build` 通过，生成头出现在 `build/Gen/ShaderHeaders/GlobalBlock.gen.h`

## 4. 首个 `.slang` 引擎 shader 与镜像切换

- [x] 4.1 新增首个真实 `.slang` 资产（`GlobalBlock.slang`：struct + `[[vk::binding(0,0)]] ParameterBlock`）
- [x] 4.2 把 `GlobalRenderResources` 的 `GlobalParams` 与 `GetGlobalBlockDesc()` 切到生成头，删除手写镜像 struct
- [x] 4.3 确认 slang 对 `ParameterBlock` 上 `[[vk::binding(b, set)]]` 的实际语义（实测 set=0/binding=0，主方案成立）

## 5. 一致性校验与测试

- [x] 5.1 新增 codegen 一致性测试：生成 C++ struct（sizeof/offsetof）vs 反射 vs `RgBlockDesc` 三方一致
- [x] 5.2 复用 `ReflectionValidation`：`GeneratedBlockDescMatchesReflection` 交叉校验生成的 `RgBlockDesc` 与 shader 反射
- [x] 5.3 `AuroraShaderTest` 覆盖虚拟 include、类型映射、反射派生 desc 场景
- [x] 5.4 全量测试绿（AuroraShaderTest 16/16、AuroraPipelineTest 7/7）

## 6. 收尾

- [x] 6.1 相关 target 全量构建通过；代码符合仓库 clang-format 风格
- [ ] 6.2 待用户确认后 `openspec archive aurora-shader-header-codegen` 归档本 change
