## Why

当前 Aurora shader 编译把 variant 全部塞进 `SessionDesc.preprocessorMacros`，每次编译新建 `ISession`，并靠 `session.detach()` 绕过 Slang `Linkage` teardown 的 use-after-free。这个组合既不符合 Slang 推荐的 session 复用方式，也把 Windows DXIL 的问题带进了所有平台。

同时，cbuffer 和 SSBO 的布局策略没有在契约里分开：cbuffer 可以逐 target 使用原生布局并由上传计划转换，SSBO 则必须让 CPU/GPU 共享同一份字节布局。当前 `ReflectBlock` 的 canonical alignment 临时补丁把这两类问题混在一起。

## What Changes

- 修复 Slang 依赖：#11614 已进入 release v2026.12+；升级到 **v2026.19**（已验证包含修复），v2026.12 作为兼容回退；删除 `session.detach()`。
- 引入独立 `Aurora.ShaderCompiler` 模块：全平台静态链接 Slang；桌面/Android/Linux/macOS 为 shared module，iOS 为 static module；其他 runtime modules 只依赖 `IShaderCompiler`/`ShaderCompilerFactory`，不 link Slang、不 include Slang 头。
- 引入 `SlangCompilerContext` 和 shader 编译线程池：
  - 编译线程池默认由 ShaderModule 托管；每 worker 独立 `IGlobalSession` 和 session pool；
  - session 只按 target/platform/environment 复用，不按宏变体拆 session；
  - 平台宏在 session 创建时固定；
  - 其他宏变体通过 source prelude 拼宏注入，sourceHash 包含拼宏结果；
  - module/artifact cache 可共享，Slang 对象不跨 worker 并发。
- Variant 设置接口全平台统一：
  - 调用方只表达 `Name→value` 和“宏变体/运行时 specialized 变体”两类；
  - 运行时 specialized 变体由后端能力决定：SPIRV/Metal 用 specialization constant，D3D12 用 root constants；
  - 平台相关宏（如 `PLATFORM_METAL`）按 target/platform 在 session 固定；
  - 其他宏变体拼进 source prelude，不进入 session key；
  - 同一 target/platform 的所有宏变体复用同一 session。
- 明确布局策略：
  - cbuffer：保留每 target 原生反射，CPU canonical schema 经现有 `UniformUploadPlan` 映射；不强制 canonical layout；
  - SSBO：强制 canonical byte layout。实测 `Std430DataLayout`/`CDataLayout` 在 Metal/DXIL 不可用，改为默认 layout + 16 字节字段/padding；复杂混合布局用 `ByteAddressBuffer`。编译/cook 期精确校验，不做 target 间转换。
- 引入预编译 Slang module（`.slang-module`）与离线 cook/artifact 路径；Develop 默认包含 ShaderModule，release 默认 cache-only、不含 ShaderModule，可通过显式选项打开运行时编译。
- 清理临时补丁：删除 `session.detach()`；撤销 `ReflectBlock` 的 canonical alignment 覆盖；修复 `sourcePath` 生命周期问题；保留 Vulkan/测试构建修复。
- 增加 session/variant/layout/thread-pool 回归测试与平台验收矩阵。
- **BREAKING**：artifact/cache/usage identity 变化，旧 shader 产品需要重新 cook；SSBO canonical layout 校验可能拒绝现有布局不符合规范的 shader。

## Capabilities

### New Capabilities

无；在既有 shader 与 resource-binding capability 内修订契约。

### Modified Capabilities

- `aurora-shader`：Slang 编译上下文与线程池、session 生命周期、统一 variant 接口（宏变体 vs 后端决定的 specialized 表达）、预编译 module、离线 cook、移除 teardown workaround。
- `aurora-resource-binding`：cbuffer 使用 per-target native layout + upload plan；SSBO 使用 16 字节 canonical layout + 精确校验；D3D12 specialized variant 使用 root constants。

## Impact

- 第三方：Slang 版本/补丁、Win32/macOS/iOS/Android/Linux 重编。
- 编译层：`ShaderCompilerSlang`、`ShaderResolver`、`ShaderCache`/`ShaderCacheWriter`、`ShaderCompileDesc`、`ShaderVariant`/schema、`ShaderUsage`、`ShaderCookTool`；新增 source prelude 拼宏与 sourceHash 规则。
- 线程：ShaderModule 内新增 shader 编译线程池、每 worker compiler context、共享 artifact cache；移除单一 `gCompilerMutex` 串行假设。
- 模块/构建：`Aurora.ShaderCompiler` 私有静态链接 Slang；Windows 仅随模块部署 dxcompiler/dxil，不再部署 slang DLL；refactor `engine/sandbox/render`、`engine/ui/render` 等直接使用 `ShaderCompilerSlang` 的模块。
- 反射/布局：`CollectSlangResources`、`ReflectBlock`、`ShaderBlockLayout`、storage layout 反射与校验、root constants/spec constants 绑定、`SharedShaderTypes`/`SharedPassHeadersTest`。
- 资产/cook：variant 元数据、module/artifact cache、shipping index。
- 平台：Windows DXIL/SPIR-V、macOS/iOS Metal、Android/Linux SPIR-V；iOS 静态 Slang、其他平台 shared。
- 关系：接续 `refactor-aurora-target-shader-artifacts`（per-target ABI、UniformUploadPlan、artifact/cache）和 `refactor-aurora-pipeline-resource-ownership`；不重做其资源归属、artifact 协议或 Name 参数事务。
- 不在本 change 内：完整 Deferred 管线、bindless/argument buffer、descriptor arrays、通用 shader 热重载、新增渲染平台。
