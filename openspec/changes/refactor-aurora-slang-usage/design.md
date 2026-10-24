## Context

当前实现：

- `ShaderCompilerSlang::Compile` 每次调用都创建 `slang::ISession`；
- variant 全部写入 `SessionDesc.preprocessorMacros`，导致 session 与宏集合绑定；
- 因为 Slang `Linkage::~Linkage` 的 operator-overload cache 跨 Linkage 共享，Windows 上多 session 编译会触发堆损坏，当前用 `session.detach()` 绕过；
- cbuffer 与 SSBO 的布局策略未分开：`ReflectBlock` 用 canonical alignment 临时抹平 DXIL 与 SPIRV/MSL 的差异；
- 编译只有一个全局互斥锁，没有 shader 编译线程池。

术语：本 change 中 **shipping** 指打包发布的产品构建，只读取 cook 后的 shader artifact/index，不依赖作者源码或运行时 Slang/DXC；**development/editor** 指允许按需编译的开发模式。

本 change 接续已实施的 `refactor-aurora-target-shader-artifacts`（per-target ABI、stage artifact、`UniformUploadPlan`、cache/shipping）和 `refactor-aurora-pipeline-resource-ownership`（资源归属、Name 参数事务）。不重做这些边界，只把 Slang 使用方式、variant 接口、线程模型和 cbuffer/SSBO 布局契约补齐。

### Existing change ownership

| 已存在 change | 保留职责 | 本 change 接续点 |
|---|---|---|
| `refactor-aurora-target-shader-artifacts` | per-target 反射、stage artifact、`UniformUploadPlan`、Name 参数事务、shipping | 不重做 artifact/绑定协议；补 compiler context/线程池、variant 接口、SSBO 精确校验 |
| `refactor-aurora-pipeline-resource-ownership` | 持久 `.slangh`、owner/inventory、cook/package | 使用其 module/usage 元数据；不改变资源归属 |
| `refactor-aurora-shared-pass-headers` | 权威 `.h`、CPU schema、无 shader 生成依赖 | 保留 canonical CPU schema；cbuffer 走 upload plan，SSBO 走 canonical layout |
| `aurora-renderer-platform-parity` | 宿主/平台生命周期 | 消费本 change 的 compiler context 和产物；不接管窗口/GPU 生命周期 |

## Goals / Non-Goals

**Goals:**

- 按 Slang 推荐方式复用 global session、session、module 和预编译产物。
- 删除 `session.detach()`，通过 backport/upgrade 修掉 teardown UAF。
- Variant 设置接口全平台统一：调用方只表达宏变体或运行时 specialized 变体；最终是 spec constant 还是 root constants 由后端能力决定。
- 宏变体尽可能复用 session：session 只按 target/platform/environment 分；平台宏在 session 创建时固定，其他宏通过 source prelude 拼宏。
- Slang 没有 per-module macro API，因此非平台宏不进入 `SessionDesc.preprocessorMacros`，而是拼进 source；这是为了满足“session 只关心 target/platform”的明确取舍。
- 支持 shader 编译线程池：每 worker 独立 Slang context/global session，共享 artifact cache。
- cbuffer 允许 per-target 原生布局；SSBO 强制 canonical byte layout 并精确校验。
- shipping 不依赖作者源码/运行时编译器；开发模式走同一 compiler context。

**Non-Goals:**

- 不重做 `refactor-aurora-target-shader-artifacts` 的 artifact/ABI/Name 参数协议。
- 不强制 cbuffer 在所有 target 上字节一致。
- 不新增 caller-facing 的 link-time constant/type 变体；本 change 只有宏变体和 specialized 变体两类。
- 不新增 bindless/argument buffer、descriptor arrays、通用 shader 热重载、新渲染平台。
- 不引入 Vulkan push constants 作为 variant 路径；Vulkan 的 specialized variant 使用 specialization constant。
- 不把 Slang 静态链接作为 teardown 修复；静态链接是 ShaderModule 的封装选择。
- 不把独立 ShaderModule 当作 teardown 隔离/修复手段；本 change 的 ShaderModule 是链接与封装边界，不是 bug 修复。
- 不在本期扩展 typed/counter/append/consume buffer 或嵌套 SSBO 结构。

## Decisions

### 1. Slang 版本与 teardown 安全

- #11614 已合入上游 master（merged commit `12f677986`），首个包含它的 release tag 为 **v2026.12**；v2026.9.2 不包含。升级基线选择 **v2026.19**（已验证包含修复），v2026.12 作为兼容回退；只有在无法升级时才 backport。
- 删除 `ShaderCompilerSlang::Compile` 的 `session.detach()`。
- 回归：连续创建/销毁 N 个 session，多轮、多 target、多 variant，无 `0xC0000374`。
- 静态链接不改变 `Linkage`/`TypeCheckingCache` 逻辑，不作为修复；独立进程只作为隔离手段。
- Slang 静态库需在 Win32/macOS/Linux/Android/iOS 重编；ShaderModule 在桌面/Android/Linux/macOS 为 shared，iOS 为 static；iOS 走 `python/third_party.py` 的静态安装流程。

### 2. ShaderModule 边界、CompilerContext 与线程池

```text
Aurora.ShaderCompiler (shared; iOS static)
  ShaderCompilePool
    worker[0] -> SlangCompilerContext -> IGlobalSession + SessionPool + ModuleCache
    worker[1] -> SlangCompilerContext -> ...
  shared ArtifactCache / ShaderCacheWriter
```

模块边界：

- `Aurora.ShaderCompiler` SHALL 是全平台唯一的 Slang 所有者；Slang SHALL 静态链接进该模块。
- 桌面/Android/Linux/macOS 使用 shared module；iOS 使用 static module（链接一次进 app）。
- 其他 runtime modules SHALL 只依赖 `IShaderCompiler` / `ShaderCompilerFactory` 等接口，SHALL NOT link `Aurora.ShaderCompiler.Static`/`3rdParty::slang`，SHALL NOT include `ShaderCompilerSlang.h`。
- Slang context/session/module cache SHALL 归 ShaderModule 所有；专用编译 pool 默认由 ShaderModule 托管，离线工具/测试可以由宿主创建专用 `ThreadPool`，但不得使用渲染并行 pool；其他模块不创建 Slang 对象。
- Windows 的 `dxcompiler.dll`/`dxil.dll` 随 ShaderModule 部署；不再部署 `slang.dll`/`slang-compiler.dll`。
- 测试和离线工具 MAY 链接私有 static compiler target；这不构成 runtime module 依赖。
- Develop 构建默认包含 ShaderModule；release 默认 cache-only、不含 ShaderModule，可通过显式选项打开；无模块时必须明确报告“无编译器”，不得静默尝试源码编译。
- 模块加载顺序/工厂注册 SHALL 明确；未加载 ShaderModule 时，需要运行时编译的调用方收到显式错误或使用预编译产物。

- `IGlobalSession` 每个 worker 一个；Slang 文档明确 global session 非线程安全，不跨 worker 共享。
- `ISession` 是 worker 内配置级对象：target/platform/environment/options/search paths/fileSystem/平台宏；同一配置复用。session key **不包含** shader 级宏变体和 runtime variant 值；使用规范化 source root/config 身份，不使用临时 fileSystem 指针。注意复用是 **worker 内**；跨 worker 不共享 session/global session。
- 线程池任务按 shader 编译请求分发；worker 内部串行使用自己的 Slang 对象；不同 worker 并发。
- 复用现有 `ShaderCompileTask` / `ShaderCompileFuture` / `sky::ThreadPool`：`RunAsync` 将任务投递到**专用 shader 编译 pool 实例**（模块托管或宿主创建），避免与渲染并行 worker 争用；worker 调用的 `IShaderCompiler` 在 ShaderModule 内使用 worker-local `SlangCompilerContext`，不共享 Slang 对象。其他模块不创建 Slang context；session 复用只在同一 worker 内成立。
- `gCompilerMutex` 的全局串行假设被移除；Slang/DXC 全局初始化用 `Once`/显式线程安全初始化。
- artifact/cache 写入必须是线程安全的共享组件；Slang module/IModule 对象不跨 worker 共享，只共享已序列化 blob/产物。
- worker 数量有上限（默认 `min(hardware_concurrency, cap)`），因为每个 global session 都会加载 core module，内存开销不可忽略；线程池接入现有 `ThreadPool`，不另建第二套调度基础设施。
- link 后的 backend codegen 可按 Slang 实验性路径并行，但本 change 不把该实验路径作为默认依赖；线程池首先保证 frontend 多线程。
- DXC/后端 codegen 的线程安全必须实测；若下游编译器不支持并发，使用每 worker 独立实例或 backend mutex，不能假定“每 worker global session”自动保证 DXC 安全。
- 编译结果与 worker 调度顺序无关；cache key/artifact 内容确定。
- worker 关闭时按 Slang 推荐顺序释放 module/session/global session；#11614 后 teardown 安全，不得依赖进程退出回收或 `detach()`。
- session pool/module cache SHALL 有上限或可显式清理；#11614 后允许安全淘汰，避免长生命周期 Editor 内存无界增长。

### 3. 统一 Variant 接口

调用方只看到两类 variant：

```text
ShaderVariantEntry
  Name key
  uint32 value
  category = CompileTimeMacro | RuntimeSpecialized
```

`category` 是**语义**，不是后端表达方式。调用方不选择 spec constant 或 root constants。

#### 3.1 宏变体（CompileTimeMacro）

- 影响代码结构、资源声明、分支折叠；
- **平台宏**（如 `PLATFORM_METAL`、`AURORA_TARGET_*`、平台/API 下限宏）在 session 创建时由 target/platform/environment 固定；
- **其他宏变体**（`ENABLE_*`、`USE_*` 等）不写入 `SessionDesc.preprocessorMacros`，而是由编译器/请求层生成 canonical source prelude：
  ```text
  #define NAME VALUE
  ...
  ```
  拼到 source 前面再编译；
- sourceHash/cache identity 包含原始 source + canonical macro prelude；
- macro 名 SHALL 是合法标识符，且 SHALL NOT 与 session/platform 宏冲突；冲突直接失败，不覆盖平台宏；
- 同一 source 的不同 macro prelude SHALL 使用不同的 module identity（module name/path 包含 prelude hash），避免同一 session 的 module cache 命中错误版本；逻辑 source path 仍用于诊断/include 解析。
- session pool key 只按 target/platform/environment，不按宏集合；
- 同一 target/platform 下的多个 shader、不同宏组合复用同一 session；
- 平台宏不重复进入 prelude，避免与 session 宏冲突；
- session 复用不等于 module 复用：不同宏组合仍产生不同 sourceHash/module，这是编译期特化的代价。

| 方案 | session key | 优点 | 代价 |
|---|---|---|---|
| 全部宏进 SessionDesc | target/platform + 宏集合 | 官方 session 语义直接 | session 随宏组合增长，与“session 只关心 target/platform”冲突 |
| 平台宏进 session，其他宏拼 source | target/platform/environment | session 最大化复用 | sourceHash/诊断/line directive 需要处理；module 仍按宏组合生成 |

本 change 选择第二种。

#### 3.2 运行时 specialized 变体（RuntimeSpecialized）

- 不改变代码结构，只改运行时值；
- artifact identity **不包含**具体值，同一 target/platform/environment 只产出一份代码；
- 后端能力决定最终表达：
  - SPIRV：用 `[SpecializationConstant]` / `vk::constant_id`；
  - Metal/MSL：用 function constant；
  - DXIL/D3D12：用 root constants；
- 后端能力由 `TargetEnvironment` 显式声明，不由调用方判断；D3D12 用 root constants，其他不支持 spec constant 的后端显式 Unsupported，不静默丢弃 variant；
- 统一设置入口：管线/材质创建时提交 `Name→value` 集合，后端 Shader/Pipeline 负责：
  - spec constant 路径：PSO 创建时写入 spec 值；
  - root constants 路径（D3D12）：写入 root signature 常量，绑定到 pipeline/draw；
- shader 作者通过生成的 variant 声明/accessor 使用：
  - spec 路径生成 `[SpecializationConstant]` 声明；
  - root constants 路径生成 `ConstantBuffer`/root constant 声明和 accessor；
  - 生成器按 `TargetEnvironment` 选择，shader 源码不出现 `#if PLATFORM_*` 的 variant 分支；
- root constants 是 D3D12 专属实现；调用方仍只提交 `ShaderVariantValues`。Vulkan 不使用 push constants，而是 specialization constant。
- root constants 受 D3D12 `D3D12_MAX_ROOT_COST=64` 的 root signature cost 预算限制：每个 root constant DWORD 计 1，descriptor table 计 1，root descriptor 计 2，static sampler 计 0；variant 值数量必须在 cook/编译期检查，超限显式失败。
- 不引入 variant cbuffer fallback；不支持 spec constant 且不是 D3D12 的后端显式 Unsupported。

#### 3.3 统一运行时设置接口

- 运行时 specialized 值 SHALL 通过统一的 `ShaderVariantValues{Name→value}` 设置；该接口不区分平台和后端。
- 管线/材质创建时提交同一个 `ShaderVariantValues`；后端 Shader/Pipeline 负责：
  - spec constant 路径：PSO 创建时写入；
  - root constants 路径（D3D12）：写入 root signature 常量；
- `CompileTimeMacro` 在编译期设置，不进入运行时 `ShaderVariantValues`；两类接口共享 `Name` 语义但不混用。

#### 3.4 选择规则

- 如果 variant 需要改变资源/字段/代码结构 → 必须是 `CompileTimeMacro`；
- 如果 variant 只是数值/开关且可运行时设置 → `RuntimeSpecialized`（Vulkan 用 spec constant，D3D12 用 root constants，Metal 用 function constant）；
- 不能把结构性 variant 静默当 specialized；
- 不能把 specialized variant 静默折叠成宏（会丢失运行时设置能力）。

#### 3.5 Schema 与身份

- `ShaderVariantSchema`（128-bit bitmask）SHALL 继续只描述 `CompileTimeMacro`；宏变体使用名称/bit 映射。
- `RuntimeSpecialized` SHALL 由独立的 specialization descriptor 描述（名称、类型、默认值、spec id 或 root constant 范围），不占用 bitmask 位，不参与 session key。
- `ShaderVariant` MAY 同时包含两类条目；编译层按 category 拆分。`Compile` 只把宏条目写入 session/code identity；runtime 条目只产生 descriptor/default，具体值在运行时设置。
- `ShaderVariantKey`/`variantHash` 对宏变体包含名称与值；对 specialized 变体只包含名称/类型/默认值和后端表达，不包含运行时具体值。
- `ShaderCacheKey` 包含 target/environment、**原始 source + macro prelude 的 sourceHash**、entry、stage、toolchain、layout/schema；不包含 session 宏集合。
- root constants 的范围/数量进入 reflection/artifact；运行时值进入 pipeline/draw 绑定，不进入代码缓存。
- `TargetEnvironment` SHALL 显式声明后端 variant 能力（例如 `supportsSpecializationConstant` / `supportsRootConstants`），供编译与管线选择；调用方不可见。
- variant 能力 SHALL 进入 TargetEnvironment/ProducerIdentity 的规范化身份和 cache key；后端能力变化必须使旧 artifact 失效。

### 4. Cbuffer 与 SSBO 布局策略

#### Cbuffer

- 每个 target 使用 Slang 原生 offset/size/alignment/stride；
- CPU 侧保持 canonical `SharedShaderTypes` schema；
- 运行时通过现有 `UniformUploadPlan` 把 CPU 字段映射到目标 offset；
- 不要求 target 间字节一致，不要求 CPU offset 等于 GPU offset；
- 删除 `ReflectBlock` 的 canonical alignment 覆盖和 block alignment 强制合并；
- `SharedPassHeadersTest` 改为校验 `BuildUniformUploadPlan` 成功，而不是 member 全等；
- 覆盖 float2/scalar 混排、矩阵、数组等不同 offset 场景；
- specialized variant 在 D3D12 走 root constants，不进入 cbuffer 布局；SPIRV/Metal 走 spec constant。

#### SSBO

SSBO 的 CPU 字节就是 GPU 读取的字节，必须强制 canonical layout：

- 实测结论（Slang v2026.9.2）：`Std430DataLayout`/`CDataLayout` 在 Metal/DXIL 不可用（E36107），不能作为跨 target 方案。
- canonical 方案：默认 layout + 强制 16 字节字段/padding。只允许 `float4`/`uint4`/`int4`/`float4x4`/这些类型的数组；禁止 scalar/float2/float3 直接混排。
- 反射实测：`float4/uint4/float4[2]/float4x4` 在 SPIRV/MSL/DXIL 的 offset/size 完全一致。
- 复杂/混合布局使用 `ByteAddressBuffer`/`RWByteAddressBuffer` 手动 offset；不引入运行期转换。
- 禁止 `float3`；统一 `float4` + 显式 padding；
- 固定宽度整数；
- 显式 `row_major`/`column_major`；
- 显式数组 stride，不依赖默认 packing；
- 不使用全局 `VulkanUseGLLayout`/`ForceScalarLayout` 顺手改布局，避免影响 cbuffer；
- `ByteAddressBuffer`/`RWByteAddressBuffer` 使用共享 layout 常量/头文件，手动 offset。

反射与校验：

- SSBO 的 CPU canonical schema SHALL 来自同一权威声明/layout 定义，不能默认复用 cbuffer 的 `SharedShaderTypes` schema；
- 扩展 `ShaderResource`/反射，记录 storage buffer 的元素 layout（size/stride/alignment/fields/array stride/matrix stride）；
- 新增 `ValidateStorageLayout(cpu, native)`：要求 offset/size/stride/alignment/matrix layout 完全一致；
- 不一致时在编译/cook 阶段明确失败，不生成运行期转换；
- 增加 CPU 写入 → GPU 读回 → CPU 校验测试。

### 5. 预编译 module 与离线 cook

- 基础 module 预编译为 `.slang-module`（`IModule::serialize` / `writeToFile` / `ISession::loadModuleFromIRBlob`）；这是 Slang 的序列化 IR translation unit，后续可脱离源码做 link/codegen。
- `.slang-module` 是 **module/translation-unit 级**产物，不按 shader instance/material 存；按 `sourceHash + target/environment/platform macros` 内容寻址，可被同一 module 的多个 entry，或 import 该库的多个 shader 共享。
- 不同 define 变体是不同 module：module identity 必须包含 canonical prelude hash；同一 session 可以加载同一 source 的多个 define 变体，但不能因 Slang module cache 同名/同 path 而返回错误版本。
- `IModule` 对象是 session/worker 局部；可共享的是序列化后的 `.slang-module` blob，不是 Slang 对象。
- 最终 target artifact 才是 per entry/stage/target/宏变体；runtime specialized 值不进入 module/artifact。
- `.slang-module` 是 target-independent IR，但仍受 Slang module 版本兼容约束（`get-supported-module-versions`）；shipping 必须固定 module/compiler 版本。
- module cache key 包含 target/environment/平台宏和**拼宏后的 sourceHash**；session 本身只按 target/platform 复用；specialized 变体不影响 module 二进制。
- session 使用 `CompilerOptionName::UseUpToDateBinaryModule` 校验新鲜度。
- 每个 worker 的 module cache 独立；共享的只是序列化 blob/artifact。
- cook 阶段完成 codegen，写入现有 cache/index；shipping 只读 artifact。
- product 模式允许无作者源码、无运行时 Slang/DXC。
- 开发模式按需编译，但仍走 compiler context/线程池。
- `ShaderUsage` 记录宏变体集合、target/environment、specialized 变体默认值/后端表达，驱动 cook 矩阵。

### 6. 平台与链接形态

| 平台 | ShaderModule | Slang | 验证重点 |
|---|---|---|---|
| Windows | shared `Aurora.ShaderCompiler.dll` | static | DXIL/D3D12、Vulkan/SPIRV、#11614 回归、线程池、session teardown、root constants、dxcompiler 部署 |
| macOS | shared `Aurora.ShaderCompiler.dylib` | static | MSL/Metal、function constant、线程池、root constants/push constants 不适用 |
| iOS | static module | static | MSL、预编译 module、设备内存/编译时间、线程池 |
| Android | shared `libAurora.ShaderCompiler.so` | static | SPIRV/Vulkan、cook 时间、只读/RW SSBO、线程池、spec constant（不使用 push constants） |
| Linux | shared `libAurora.ShaderCompiler.so` | static | SPIRV/Vulkan、offscreen 验收、线程池、spec constant（不使用 push constants） |

- 不把静态链接作为 teardown 修复；静态 Slang 只是 ShaderModule 的封装方式。
- 每平台重建静态 Slang 包；Windows 只随模块部署 dxcompiler/dxil，不部署 slang DLL。
- `ISession::loadModuleFromSourceString` 返回的 module 由 session 的 module cache 持有；调用方只借用，禁止用 `ComPtr::attach` 接管所有权，否则 session 析构时引用计数损坏。
- iOS 静态 module 链接一次进 app；桌面/Android/Linux/macOS 为 shared module。
- 后端是否支持 specialization constant 进入 `TargetEnvironment` 能力，不作为调用方分支。

### 7. 与现有实现/临时补丁的关系

- 删除 `session.detach()`（Phase 1 完成后）。
- 撤销 `ReflectBlock` canonical alignment/block alignment 覆盖，改为 per-target native cbuffer + upload plan。
- 保留 `sourcePath` 生命周期修复、Vulkan `range=0` 显式失败测试、`RendererRuntimeTest` 编译修复。
- `sky_add_test` 的 link 签名问题可另行统一，不混入本 change。
- 不修改 `refactor-aurora-target-shader-artifacts` 已完成的 artifact/ABI 协议；只追加 storage layout、variant 接口、模块边界和线程模型。
- refactor `engine/sandbox/render`、`engine/ui/render` 等直接实例化 `ShaderCompilerSlang` 的模块，改为 factory/service 或预编译产物；其他 runtime modules 不再 link Slang。

### 8. 验收与迁移

- 阶段：
  1. Slang #11614/升级 + 删除 detach；
  2. CompilerContext + 编译线程池；
  3. 统一 variant 接口 + 宏 session 复用 + specialized 后端表达；
  4. cbuffer/SSBO 布局策略；
  5. 预编译 module/offline cook；
  6. 平台/回归/文档。
- 验收：
  - N 次 session 创建/销毁无堆损坏；
  - 线程池并发编译不同 shader 无数据竞争/崩溃；
  - 同一 target/platform 的多个 shader（不同宏组合）复用 session；
  - 平台宏在 session 创建时固定；
  - 宏变体通过 source prelude 拼宏，sourceHash 变化可失效缓存；
  - specialized variant 在 SPIRV/Metal 走 spec constant，在 D3D12 走 root constants，调用方接口不变；
  - cbuffer upload plan 在 SPIRV/MSL/DXIL 全部通过；
  - SSBO 三 target 反射 offset/size/stride 与 CPU schema 完全一致；
  - shipping 无源码/编译器可读；
  - 产物与旧路径逐 target 对比；
  - 编译时间/内存基准记录（含 worker 数量和 session 数）。

### 9. API 草图（概念约束，不是最终签名）

```text
ShaderVariantCategory { CompileTimeMacro, RuntimeSpecialized }

ShaderVariantEntry {
    Name key;
    uint32 value;
    ShaderVariantCategory category;
}

ShaderVariantValues {          // runtime, platform-agnostic
    vector<ShaderVariantEntry> entries; // category == RuntimeSpecialized
}

TargetEnvironment {
    ...
    VariantCapabilities variant; // specialization constants? root constants? spec id range
}

SlangCompilerContext {
    IGlobalSession global;
    SessionPool sessions;      // ShaderModule/worker-local, keyed by target/platform/env (no macro set)
    ModuleCache modules;       // worker-local
}

ShaderCompilePool {
    // dedicated; module-hosted or host-owned ThreadPool
    Submit(ShaderCompileTask) -> worker calls IShaderCompiler;
}

SlangCompilerContext {         // thread-local / per worker inside module
    IGlobalSession global;
    SessionPool sessions;
    ModuleCache modules;
}

ShaderStorageLayout {          // reflected SSBO element layout
    size, stride, alignment;
    fields: name/offset/size/type/arrayStride/matrixStride;
}
```

约束：

- `ShaderVariantValues` 不暴露 backend 选择；DXIL 写 root constants，Metal/SPIRV 写 spec constant。
- `SlangCompilerContext` 不跨 worker 共享（thread-local/per worker）；`ArtifactCache` 是共享的线程安全边界。
- `ShaderStorageLayout` 只用于 SSBO 精确校验；cbuffer 仍使用 `ShaderBlockLayout` + `UniformUploadPlan`。

### 10. 测试方案

#### SSBO 布局

1. 16 字节 canonical struct：`float4`/`uint4`/`int4`/`float4[]`/`MAT4`，显式 padding；编译 SPIRV/MSL/DXIL，确认 offset/size 一致（已用反射验证 `a0,b16,c32,e48,m80`）。
2. GPU 读回：CPU 写入 → shader 读取/写回 → CPU 校验；覆盖 `StructuredBuffer` 和 `RWStructuredBuffer`。
3. 在 Windows/D3D12、Windows/Linux Vulkan、macOS/iOS Metal、Android Vulkan 分别执行。
4. 负例：scalar/float2/float3 混排、数组、矩阵 major 交换；确认会被校验拒绝或必须走 `ByteAddressBuffer`。
5. 复杂布局用 `ByteAddressBuffer` 手动 offset，单独测试 offset 一致性。

#### Cbuffer

- 每 target 反射保留原生 offset/alignment；
- `BuildUniformUploadPlan` 在 SPIRV/MSL/DXIL 全部成功；
- float2/scalar/matrix/array 混排；
- 删除 canonical alignment 后回归 SharedPassHeaders。

#### Root constants（D3D12 实现路径）

- D3D12 root signature DWORD 预算/溢出；
- per-pipeline vs per-draw 更新；
- 记录 root constants 的 CPU/GPU 开销；
- 多 draw/indirect/多帧；root signature DWORD 超限时显式失败。

#### 线程池

- 并发编译不同 shader，结果与调度顺序无关；
- session 数只随 target/platform 变化，不随宏组合；
- source prelude/sourceHash 稳定；
- DXC/codegen 并发安全；
- worker 关闭/缓存清理无泄漏、无 UAF。

#### Define 变体 module identity

- 同一 session 依次加载同一 source 的 `ENABLE_X=0` 与 `ENABLE_X=1`；
- 确认 module identity/sourceHash 不同，返回两个 module/artifact，不发生 Slang module cache 误命中；
- 确认逻辑 sourcePath/诊断行号仍可读。

#### 模块边界

- `dumpbin/otool/readelf` 检查其他 runtime modules 不依赖 Slang；
- ShaderModule static Slang；Windows 无 slang DLL，dxcompiler/dxil 随模块；
- iOS static module 只链接一次；
- ShaderModule 未加载时 factory 显式失败或使用预编译产物。

#### Shipping

- 移除作者源码、Slang/DXC 后加载 artifact；
- 缺失/不兼容产物显式失败；
- cache-only 产品可省略 ShaderModule。

## Risks / Trade-offs

- [每个 worker 一个 global session 内存开销大] → 线程池有上限；记录每 worker 内存；允许按任务批量复用。
- [宏变体不再进入 session] → 非平台宏拼进 source，平台宏在 session 固定；session 只按 target/platform/environment 分，最大化复用。
- [D3D12 root constants 预算] → cook/编译期检查 DWORD 数量，超限失败；不引入 variant cbuffer fallback。
- [source prelude 影响行号/诊断] → 统一 canonical prelude、保持 `LineDirectiveMode=None` 或生成 line directive；sourceHash 包含 prelude，避免缓存碰撞。
- [宏名冲突] → prelude 只包含 schema 中声明的宏；平台宏由 session 固定，禁止重复定义。
- [后端能力差异] → `TargetEnvironment` 显式能力；D3D12 用 root constants，其他不支持 spec constant 的后端显式 Unsupported，不静默降级。
- [SSBO 强制布局可能要求改 shader/CPU 结构] → 先在内置 buffer 上落地，提供编译/cook 诊断；不保留多套隐式布局。
- [线程池与 Slang 非线程安全] → 每 worker 独立 context；不共享 session/global session；共享 artifact 写入加锁。
- [DXC/后端 codegen 线程安全未知] → 先做并发实测；必要时每 worker 独立 DXC 实例或 backend mutex，不能让 frontend 线程池默认依赖 DXC 并发安全。
- [跨平台重编 Slang 成本] → 按平台矩阵分批；先 Windows，再 macOS/iOS/Android/Linux。
- [ShaderModule 静态 Slang 体积] → 只在一个模块内链接一次；shipping cache-only 可省略模块。
- [模块加载顺序/工厂为空] → 显式错误或使用预编译产物；需要运行时编译的调用方必须在 ShaderModule 初始化后运行。
- [静态 Slang + dxcompiler] → Windows 仍随模块部署 dxcompiler/dxil；验证 DXC 并发与部署路径。
- [backport #11614 依赖其他上游提交] → 先做小范围 build gate；若依赖过深，优先升级到包含该提交的 Slang 版本。
- [cache/artifact identity 变化] → 旧产品需要重新 cook；版本号/分发清单显式升级，不静默复用旧产物。
- [SSBO 规范收紧] → 现有不符合 canonical layout 的 shader 会被拒绝；先内置 buffer 迁移并提供诊断，再推广到插件。

## Open Questions（按优先级）

### P0（实施前必须定）

- SSBO canonical 采用“默认 layout + 16 字节字段”还是 `ByteAddressBuffer`；必须完成 Metal + DX12 + Vulkan GPU 读回验证。
- Slang 升级到 v2026.19（v2026.12 回退）或 v2026.9.2 backport，以及 Windows/macOS/Linux/Android/iOS 静态包能否一次打通。

### P1（首轮实现内解决）

- root constants 数量与 root signature DWORD 预算检查；D3D12 pipeline 绑定 seam。
- source prelude 由编译请求层还是 `ShaderHeaderTool`/cook 生成；诊断行号/include 体验。
- ShaderModule 加载顺序与 factory 为空时的行为。
- DXC/后端 codegen 并发安全实测。
- Develop 默认带 ShaderModule；release 默认不带、显式开关打开。

### P2（低优先级，可后置）

- 线程池默认 worker 数、global session 内存预算调优。
- ShaderModule 静态 Slang 体积/启动开销优化。
- SSBO `ByteAddressBuffer` 复杂布局工具链完善。
