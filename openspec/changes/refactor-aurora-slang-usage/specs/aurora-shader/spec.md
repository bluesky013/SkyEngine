## MODIFIED Requirements

### Requirement: 统一变体模型

aurora.shader SHALL 提供全平台统一的 `Name→value` variant 设置接口，并只暴露两类语义：

- `ShaderVariantEntry{Name key, uint32 value, ShaderVariantCategory category}`；
- `category = CompileTimeMacro | RuntimeSpecialized`，未指定时 SHALL 默认 `CompileTimeMacro` 以兼容现有资产；
- `ShaderVariant` — 一次具体 variant 选择（entries 列表）；编译只消费宏条目，runtime 条目只产生 descriptor/default。
- `ShaderVariantSchema`（128-bit bitmask）SHALL 只描述 `CompileTimeMacro`；`RuntimeSpecialized` SHALL 使用独立的 specialization descriptor（名称/类型/默认值/spec id 或 cbuffer 字段），不占用 bitmask。
- `TargetEnvironment` SHALL 声明后端 variant 能力（例如是否支持 specialization constant、是否使用 root constants），供编译和管线选择，不暴露给调用方；该能力 SHALL 进入环境身份和 cache key，能力变化使旧 artifact 失效。
- Artifact/reflection SHALL 记录 `RuntimeSpecialized` descriptor（名称/类型/默认值）以及最终表达（spec id 或 root constant 范围）；运行时值本身不进入 artifact。
- 运行时 `RuntimeSpecialized` 值 SHALL 通过统一的 `ShaderVariantValues{Name→value}` 提交；调用方 SHALL NOT 选择 specialization constant 或 root constants，最终表达由后端能力决定。

#### Scenario: 统一设置接口

- **WHEN** 调用方为同一 shader 设置 `{"USE_SHADOWS", 1}` 与 `{"NUM_LIGHTS", 4}`
- **THEN** 使用同一个 `ShaderVariant` 接口；不出现 Vulkan/DXIL/Metal 专用设置 API

#### Scenario: 两类语义

- **WHEN** 检查 variant entry
- **THEN** 结构性开关 SHALL 标为 `CompileTimeMacro`，运行时数值 SHALL 标为 `RuntimeSpecialized`；不得由调用方指定 spec/root constants 实现

#### Scenario: schema 分工

- **WHEN** 同一 shader 同时有宏变体和 specialized 变体
- **THEN** 宏变体 SHALL 进入 bitmask schema 和 source prelude；specialized 变体 SHALL 进入独立 descriptor，不占用 bit 位

#### Scenario: 统一运行时设置

- **WHEN** 在 Windows/DXIL 与 macOS/Metal 上设置同一个 specialized variant
- **THEN** 调用方使用同一个 `ShaderVariantValues`；DXIL 通过 root constants，Metal 通过 function constant，接口不变

### Requirement: 机制由 shader 声明决定

编译和管线创建 SHALL 按以下规则解析 variant：

1. `CompileTimeMacro`：
   - 平台宏（如 `PLATFORM_METAL`、`AURORA_TARGET_*`）在 session 创建时由 target/platform/environment 固定；
   - 其他宏变体 SHALL 拼进 source prelude（`#define NAME VALUE`），不进入 `SessionDesc.preprocessorMacros`，也不进入 session key；
   - macro 名 SHALL 是合法标识符，且 SHALL NOT 与平台/session 宏冲突；冲突必须失败，不得覆盖平台宏；
2. `RuntimeSpecialized` → 后端能力决定：
   - SPIRV/Metal 等支持 specialization constant 时，生成/使用 `[SpecializationConstant]` / `vk::constant_id` / function constant；Vulkan 不使用 push constants 作为 variant 路径；
   - DXIL/D3D12：使用 root constants；其他不支持 spec constant 且非 D3D12 的后端显式 Unsupported；
3. shader 作者通过生成的 variant 声明/accessor 使用，不直接写 backend-specific 分支。

结构性 variant SHALL NOT 被静默当作 `RuntimeSpecialized`；`RuntimeSpecialized` SHALL NOT 被静默折叠成宏。SPIRV/Metal 使用 spec constant；D3D12 使用 root constants；其他不支持的后端显式 Unsupported，不得丢弃值。

#### Scenario: 宏变体

- **WHEN** variant 为 `CompileTimeMacro`
- **THEN** 平台宏进入 session；其他宏进入 source prelude；同一 target/platform 的不同宏组合复用同一 session

#### Scenario: spec constant 路径

- **WHEN** 后端支持 specialization constant 且 variant 为 `RuntimeSpecialized`
- **THEN** 产物包含 spec 声明；管线创建时写入值，artifact identity 不包含运行时具体值

#### Scenario: D3D12 root constants

- **WHEN** 后端为 D3D12 且 variant 为 `RuntimeSpecialized`
- **THEN** 使用 root constants；调用方接口不变，artifact 仍不包含运行时值；root signature DWORD 预算 SHALL 校验

#### Scenario: 平台宏固定

- **WHEN** 当前 target 是 iOS/Metal
- **THEN** `PLATFORM_METAL` 等平台宏在 session 创建时确定，不由每个 shader 的 variant 重复设置

### Requirement: Slang 编译通道

`ShaderCompilerSlang` SHALL 经 Slang Compilation API（`IGlobalSession` / `ISession` / module load / compose+link / `getEntryPointCode`）把 Slang 源码编译为目标产物；实现 SHALL 位于 `Aurora.ShaderCompiler` 模块内，Slang 静态链接进该模块。

- `Aurora.ShaderCompiler` SHALL 是全平台唯一的 Slang 所有者：桌面/Android/Linux/macOS 为 shared module，iOS 为 static module；其他 runtime modules SHALL 只依赖 `IShaderCompiler`/`ShaderCompilerFactory`，SHALL NOT link Slang 或 include `ShaderCompilerSlang.h`。Vulkan 的 specialized variant 使用 specialization constant，不使用 push constants。
- `IGlobalSession` SHALL 为每 worker 一个；不得所有 worker 共享一个 global session。
- `ISession` SHALL 代表一组一致配置（target/platform、options、search paths、file system、平台宏），按配置在 worker 内复用；session key SHALL NOT 包含 shader 级宏变体或 runtime variant 值；target/platform 是配置的一部分，但不得按 shader/stage 无界新建。
- 非平台宏变体 SHALL 通过 source prelude 拼宏；sourceHash 包含原始 source + canonical prelude；不同宏组合 SHALL 产生不同 sourceHash/module，session 复用不等于 module 复用。
- 同一 source 的不同 macro prelude SHALL 使用不同的 module identity（module name/path 包含 prelude hash），同一 session 加载多个 define 变体时 SHALL NOT 发生 module cache 误命中。
- module SHALL 优先从预编译 `.slang-module` 加载，再回退源码；module cache key 包含 target/environment/平台宏和拼宏后的 sourceHash。
- 反射 SHALL 收敛到统一 `ShaderReflection`；逻辑 `set/binding` 与目标原生地址分离，不得假设 `native.space == logical set`。Vulkan 使用 set/binding；DXIL 使用原生 register space（`[[vk::binding]]` 可能折叠为 space 0）；Metal 使用 DirectArguments 槽位。
- 虚拟 include 继续经 `ShaderFileSystem` 解析，不要求落盘。
- SHALL 不依赖 `session.detach()` 规避 teardown；session 生命周期由 Slang 修复后的正常释放保证。

#### Scenario: worker 内 session 复用

- **WHEN** 同一 worker 编译多个使用相同 target/platform/environment、但不同宏变体的 shader
- **THEN** 复用同一 `ISession`；宏变体只改变 source prelude/sourceHash，不产生每宏组合一个 session；跨 worker 仍不共享 session/global session

#### Scenario: 预编译 module

- **WHEN** 基础 module 的 `.slang-module` 存在且新鲜
- **THEN** 编译 SHALL 优先加载该 module，不再 parse/type check 源码

#### Scenario: 逻辑地址与原生地址分离

- **WHEN** DXIL 编译含 `[[vk::binding(0,2)]]` 的 ConstantBuffer
- **THEN** 逻辑 set 保持 2，D3D12 root signature 使用 Slang 实际产出的 native space/index；不得强制把 native space 改成 2

#### Scenario: 无 detach 的 teardown

- **WHEN** 连续编译并正常销毁多个 session
- **THEN** 不出现 use-after-free/堆损坏；代码中不存在 `session.detach()` 作为 workaround

### Requirement: 统一 cache key 与 ShaderCache 接口

`ShaderCacheKey` SHALL 统一并扩展为 `{sourceHash, variantHash, target, stage, entryHash, layoutFp, schemaFp, toolchainFp}`；`sourceHash` 对宏变体 SHALL 包含原始 source + canonical macro prelude；`variantHash` 对 `CompileTimeMacro` 包含名称与值，对 `RuntimeSpecialized` 只包含名称/类型/默认值（如果默认值影响代码），不包含运行时具体值。`ShaderCache`（Load/Store）接口 SHALL 继续由内容寻址存储后端承载（offline 只读 / local 可写两 root）；`ShaderCompileDesc` 的 `cache` 字段 SHALL 被使用。

#### Scenario: 统一 key

- **WHEN** 同一 source + 同一 variant + 同一 target + 同一 layout/schema/toolchain 编译两次
- **THEN** 产生相同 `ShaderCacheKey`；`cache == nullptr` 时走直接编译

#### Scenario: 宏值影响身份

- **WHEN** `CompileTimeMacro` 名称或值不同
- **THEN** `ShaderCacheKey` 不同，不命中旧产物

#### Scenario: 运行时值不影响代码身份

- **WHEN** 只有 `RuntimeSpecialized` 的值不同
- **THEN** 不重新编译代码；值由 spec constant 或 root constants 在运行时绑定

## ADDED Requirements

### Requirement: Slang 版本与 teardown 安全

项目 SHALL 使用包含上游 #11614（移除 operator overload type-checking cache）的 Slang，或以补丁形式 backport 到当前 pinned 版本。该修复 SHALL 进入所有平台的 Slang 包；不得以静态链接、独立 DLL 或进程内 session pool 代替版本修复。

#### Scenario: 多轮编译不损坏堆

- **WHEN** 同一进程连续创建/销毁 N 个 session，覆盖 SPIRV/MSL/DXIL、VS/FS/CS、多个 variant 与 optimization
- **THEN** 不出现 `0xC0000374` 或 `Free Heap block ... modified after it was freed`

#### Scenario: 移除 workaround

- **WHEN** Slang 修复生效
- **THEN** `ShaderCompilerSlang` 不再调用 `session.detach()`；session 正常释放

### Requirement: Shader 编译线程池

Aurora SHALL 支持多线程 shader 编译：

- ShaderModule SHALL 持有 Slang context/session/module cache；专用编译 pool 默认由模块托管，离线工具/测试可由宿主创建，但不得使用渲染并行 pool；每个 worker SHALL 拥有独立的 `SlangCompilerContext`（`IGlobalSession` + session pool + module cache）；其他模块不创建 Slang context；
- Slang global session/session/派生对象 SHALL NOT 跨 worker 并发共享；
- artifact/cache 写入 SHALL 线程安全、顺序无关；
- worker 数量 SHALL 有上限并可配置；默认不超过硬件并发数，且记录 global session 内存开销；
- 编译任务 SHALL 可在任意 worker 执行，结果与调度顺序无关；
- 全局编译初始化（如 DXC 定位）SHALL 使用线程安全的一次性初始化；
- 线程池 SHALL 复用现有 `ShaderCompileTask` / `ShaderCompileFuture` / `sky::ThreadPool`，使用专用 shader 编译 pool 实例，避免与渲染并行 worker 争用；
- DXC/后端 codegen 的并发安全 SHALL 验证；不支持时使用每 worker 独立实例或 backend mutex，不得假定自动安全；
- worker 关闭 SHALL 按 Slang 推荐顺序释放 module/session/global session；不得依赖进程退出回收或 `detach()`；
- session pool/module cache SHALL 有上限或可显式清理，避免长生命周期进程内存无界增长。

#### Scenario: 并发编译

- **WHEN** 两个 worker 同时编译不同 shader
- **THEN** 各自使用独立 global session/session；不得出现数据竞争、崩溃或跨 worker 的 Slang 对象共享

#### Scenario: 结果确定

- **WHEN** 同一批编译请求以不同 worker 调度顺序执行
- **THEN** artifact/cache 内容与 identity 相同

#### Scenario: session 复用

- **WHEN** 同一 worker 编译同一宏配置的多个 shader
- **THEN** 按宏配置复用 session；session 数量不随 shader 数量线性增长

### Requirement: Shader 编译模块边界

Aurora SHALL 通过独立 `Aurora.ShaderCompiler` 模块封装 Slang：

- 模块 SHALL 静态链接 Slang；桌面/Android/Linux/macOS 为 shared module，iOS 为 static module；
- Windows 的 dxcompiler/dxil SHALL 随模块部署；不得部署 `slang.dll`/`slang-compiler.dll`；
- 其他 runtime modules SHALL 不 link `Aurora.ShaderCompiler.Static`/`3rdParty::slang`，不 include Slang/编译器实现头；
- 测试和离线工具 MAY 链接私有 static compiler target；
- Develop 构建 SHALL 默认包含 ShaderModule；
- release/shipping SHALL 默认使用预编译产物、不包含 ShaderModule/Slang；SHALL 提供显式选项允许打开运行时编译，打开时按需部署 ShaderModule、源根和 dxcompiler/dxil；
- shipping 完全使用预编译产物时必须显式报告无编译器，不得回退源码编译；
- 未加载 ShaderModule 时，需要运行时编译的调用方 SHALL 收到显式错误或使用预编译产物。

#### Scenario: 模块唯一所有者

- **WHEN** 检查生产 runtime modules 的链接依赖
- **THEN** 只有 `Aurora.ShaderCompiler` 链接 Slang；其他模块通过接口/工厂使用编译器

#### Scenario: 模块形态

- **WHEN** 构建 Windows/macOS/Linux/Android
- **THEN** ShaderModule 为 shared library 且静态链接 Slang；iOS 为 static module

#### Scenario: shipping 无编译器

- **WHEN** shipping 省略 ShaderModule
- **THEN** 已 cook 产物可加载；缺失产物时显式失败，不尝试源码编译

### Requirement: 预编译 module 与离线 cook

Aurora SHALL 支持将基础 Slang module 预编译为 `.slang-module`，并在 cook 阶段完成 codegen 与 artifact 写入。shipping SHALL 能够在不包含作者源码、运行时 Slang/DXC 的情况下加载兼容产物；开发模式 MAY 按需编译，但必须走同一 compiler context/线程池。

module cache key SHALL 包含 target/environment/平台宏和拼宏后的 sourceHash；`RuntimeSpecialized` 不改变 module 二进制。`.slang-module` SHALL 是 module/translation-unit 级产物，按内容寻址；同一 module 的多个 entry 或 import 该 module 的多个 shader 可共享；SHALL NOT 按 material/shader instance 存储。

#### Scenario: 无编译器产品

- **WHEN** shipping 包移除作者源码和运行时编译器
- **THEN** 已 cook 的 shader 仍可按 artifact/index 加载并执行；缺失或不兼容时明确失败，不回退源码

#### Scenario: 新鲜度失效

- **WHEN** module 源码、target 环境、平台宏或 ProducerIdentity 变化
- **THEN** `UseUpToDateBinaryModule`/cache 判定失效并重新编译，不复用旧 `.slang-module`
