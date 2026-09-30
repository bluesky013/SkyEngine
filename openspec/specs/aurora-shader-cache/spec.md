# aurora-shader-cache Specification

## Purpose
TBD - created by archiving change aurora-shader-cache. Update Purpose after archive.
## Requirements
### Requirement: Shader 身份为相对路径（非资产）

Shader 引用 SHALL 为 `ShaderRef { std::string relativePath; std::string entry; }`，以 `relativePath` 作为身份，由 material / pass 引用。Shader SHALL NOT 注册 `AssetTraits`，SHALL NOT 作为序列化资产存在。

`relativePath` SHALL 相对某个配置的源码搜索根，保证跨机器一致；解析前 SHALL 归一化（`/` 分隔、大小写归一等）。

#### Scenario: 引用与解析键

- **WHEN** material 引用 `ShaderRef{"material/lit.slang", "FSMain"}`
- **THEN** 解析以归一化后的 `relativePath` 为键；`entry` 用于选取模块中的阶段

#### Scenario: 非资产

- **WHEN** 查找 `AssetTraits<Shader>`
- **THEN** 不存在；Shader 不走资产系统

### Requirement: 三段解析（schema / 变体 / 二进制）

解析 SHALL 分三段：`ResolveSchema(path)`（取得 schema 与 schemaFp）、`Assemble(...)`（算出 variantKey / spec / variantHash）、`ResolveShader(path, variantInfo, target)`（取得二进制）。`ResolveShader` SHALL NOT 依赖 schema 来计算 hash。

#### Scenario: schema 与二进制解耦

- **WHEN** 调用 `ResolveShader`
- **THEN** 只需 `variantInfo.variantHash` 与 `target`，不需要 schema

### Requirement: 解析优先级 离线 > 源码 > 本地实时

`ResolveShader` SHALL 严格按 **offline root > 源码现场编译 > local root** 顺序解析。有源码时缓存产物 MUST 满足 `sourceHash` 匹配才可用；无源码时只要求 blob 存在。

#### Scenario: 离线缓存命中

- **WHEN** offline root 存在与当前 `sourceHash` 匹配的产物
- **THEN** 直接加载该 blob，不编译

#### Scenario: 源码变更触发现场编译

- **WHEN** 源码改动使 `sourceHash` 变化，且编译器可用
- **THEN** 走现场编译并写回 local root

#### Scenario: 无源码命中本地缓存

- **WHEN** 无源码可定位，local root 存在对应产物
- **THEN** 加载 local blob

#### Scenario: 无编译器且 miss 报错

- **WHEN** 未注册编译器且 offline / local 均未命中
- **THEN** 返回错误（不做降级）

### Requirement: 内容寻址缓存（单 index + blobs）

缓存 SHALL 为内容寻址结构：单个版本化 `index.bin`（含 `formatVersion` / `toolchainFp` / `layoutFp` 与 `map<normalizedPath, PathEntry>`；`PathEntry` 含 `schemaFp` / `schema` / `entryPoints` / `artifacts`）与内容寻址的 `blobs/<compileHash>`（含 `reflection` + `StageBlob[]`）。

SHALL 支持多 root（offline 只读 / local 可写），多 root SHALL 经 `core::MultiFileSystem` 按优先级 mount 实现，解析时 offline 优先；写入只写 local。

存储 SHALL 按 `ShaderTarget` 分为独立子目录（`vulkan` / `metal` / `d3d12`），各自持有 `index.bin` 与 `blobs/`，便于按 backend 打包与清理。

#### Scenario: 按 target 隔离

- **WHEN** 同一 shader/variant 分别以 SPIRV / MSL / DXIL 目标编译
- **THEN** 产物分别落在 `<root>/vulkan`、`<root>/metal`、`<root>/d3d12` 下，互不覆盖

#### Scenario: 层级失效

- **WHEN** `index.layoutFp` 与当前 `GlobalVariantLayout` 不符
- **THEN** 该 root 整份 index 视为无效

#### Scenario: schema 失效

- **WHEN** `PathEntry.schemaFp` 与当前 schema 不符
- **THEN** 该 path 的 artifacts 全部失效

#### Scenario: 内容寻址去重

- **WHEN** 两处不同 `relativePath` 内容相同、变体相同、target 相同
- **THEN** 共享同一 `blobs/<compileHash>`

### Requirement: 编译键与 entry 级产物

`compileHash` SHALL 为 `H(sourceHash, layoutFp, schemaFp, variantHash, target, stage, entryHash, toolchainFp)`。v1 SHALL 为 **entry 级**：每次编译产出一个 entry 的 `data` + reflection，缓存按 entry 独立存储。

（module 级——模块一次产出全部 entry、reflection 只存一份——为后续增强，v1 不做。）

`sourceHash` SHALL 覆盖 include 闭包与 companion。

#### Scenario: entry 改变 compileHash

- **WHEN** 同一 source / variant / target，但 `entry` 或 `stage` 不同
- **THEN** `compileHash` 不同（各自独立的 blob），不会互相覆盖

#### Scenario: include 变更失效

- **WHEN** 被 include 的文件内容变化
- **THEN** `sourceHash` 变化，缓存不命中

### Requirement: 变体 key 组装

变体 key SHALL 为 128-bit `ShaderVariantKey`，分区为 `pipeline 保留区 P | 顶点语义区 V | per-shader schema 区 S`，且 SHALL 满足 `P+V+S <= 128`。

组装 SHALL 由 `ShaderVariantAssembler` 完成：pipeline 位来自 `GlobalVariantLayout`，顶点语义来自几何 `VertexSemanticMask`，schema 条目值来自 material 覆盖或 default；`isSpec` 条目 SHALL 进 `ShaderSpecialization`。v1 SHALL 把 spec 值并入 `variantHash`（不做 moduleKey / psoKey 分层）。

#### Scenario: 顶点语义位

- **WHEN** 几何顶点流含 COLOR 且 schema 声明 `HAS_VERTEX_COLOR : COLOR`
- **THEN** 对应位被置位并进 `variantHash`

#### Scenario: material 覆盖与默认

- **WHEN** schema 声明 `USE_SHADOWS` 默认 0，material 覆盖为 1
- **THEN** key 中该位取 1；未覆盖条目取 default

#### Scenario: 越界报错

- **WHEN** schema 的 `totalBits` 使 `P+V+S > 128`
- **THEN** 组装失败并报错，不静默截断

### Requirement: schema 载体为 companion `.slang.json`

per-shader schema SHALL 由与源码同目录的 companion `.slang.json` 声明，且 SHALL 计入 `sourceHash`。schema 解析 SHALL 遵循 **源码 companion > 离线 index**。

#### Scenario: companion 优先

- **WHEN** 源码旁存在 companion 且离线 index 也有该 schema
- **THEN** 使用 companion 的 schema 与 schemaFp

#### Scenario: 无 companion 用离线

- **WHEN** 无源码 / 无 companion，离线 index 有 schema
- **THEN** 使用离线 schema

### Requirement: 单写者 IO 与原子落盘

本地缓存写入 SHALL 由单一 IO 线程串行执行（`ShaderCacheWriter`）：生产者 SHALL 经提交队列投递，SHALL NOT 在调用线程直接读改写索引。索引与载荷落盘 SHALL 使用「临时文件 + 重命名」原子替换（不支持 rename 时退化为直写）。并发写入 SHALL NOT 丢失条目。

#### Scenario: 并发提交不丢条目

- **WHEN** 多个线程并发提交不同 artifact
- **THEN** flush 后全部条目可从索引查到

#### Scenario: 原子替换

- **WHEN** 写 `index` / `blob`
- **THEN** 先写临时文件再重命名，读者不会读到半截文件

### Requirement: 编译器抽象与可选模块

SHALL 定义抽象 `IShaderCompiler` 与 `ShaderCompilerFactory`（注册）。编译实现 SHALL 位于独立模块并注册进 factory；该模块 SHALL 可带也可不带——带上时缓存 miss 可现场编译兜底，不带时仅走缓存。

#### Scenario: 带编译器可兜底

- **WHEN** 已注册编译器且缓存 miss
- **THEN** 现场编译成功并返回

#### Scenario: 不带编译器仅缓存

- **WHEN** 未注册编译器且缓存未命中
- **THEN** 明确报错；已有缓存仍可正常解析

### Requirement: 编译任务可内联或异步调度

Resolver SHALL 把编译拆成自包含的 `ShaderCompileTask`，由调用方选择执行线程：内联（当帧 render parallel，阻塞、不可跳过）或异步（worker，`Ready()` 前可跳过）。编译产物 SHALL 经 `CommitCompile` 落缓存（writer 队列或同步）。

#### Scenario: 异步编译

- **WHEN** `ResolveShaderAsync` 返回 `Compile` 且调用方 `RunAsync(pool)`
- **THEN** `Ready()` 后 `Succeeded()` 为真，且结果已提交缓存

#### Scenario: 内联编译

- **WHEN** 得到 `Compile` 后调用 `RunInline()`
- **THEN** 立即完成并提交缓存

### Requirement: 运行时 Shader 创建

解析得到的 blob + reflection SHALL 经 `Device::CreateShaderFunction` / `CreateShader` 创建 RHI `Shader`；该创建 SHALL 在 render 线程进行（Device 独占）。

#### Scenario: 从 blob 建 Shader

- **WHEN** 解析出 blob 与 reflection
- **THEN** 在 render 线程创建 `ShaderFunction`（vs/ps）与 `Shader`

### Requirement: Shader 使用收集（离线预编）

系统 SHALL 支持运行时收集 shader 使用（`ShaderUsageEntry { relativePath, entry, stage, target, variantKey, variantHash, variantDump, toolchainFp }`），按 `(relativePath, entry, target, variantHash)` 去重，并跨会话 **union 合并**写入 `shader_usage.json`。离线工具 SHALL 按 `(relativePath, target, variantHash)` 聚合 entry 并集，按 module 编译并产出 `index.bin` / `blobs`。

收集 SHALL 由开关控制（dev / editor 默认开，发布默认关）。

#### Scenario: 记录 resolve

- **WHEN** 一次 resolve 发生（无论命中与否）且收集开启
- **THEN** 记录一条 usage entry（去重）

#### Scenario: union 合并

- **WHEN** 两次会话收集到不同条目
- **THEN** flush 后文件包含两者并集且无重复

#### Scenario: 离线按模块聚合

- **WHEN** usage 含同一 module 的多个 entry
- **THEN** 离线工具聚合 entry 并集并一次编译该 module

