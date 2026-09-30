## Context

Aurora 的 shader 侧现状：

- `aurora/shader`：`ShaderCompilerSlang` 可离线把 `.slang` 编译为 SPIRV / DXIL / MSL 并反射出
  `ShaderReflection`；`ShaderVariant` / `ShaderVariantSchema` / `ShaderVariantKey`（128-bit）/
  `BuildVertexVariant` 已存在；`aurora/pipeline/GlobalVariantLayout` 预留了 pipeline 变体位。
- `aurora/rhi`：`ShaderCacheKey { sourceHash, variantHash, target }` 已定义；
  `Device::CreateShaderFunction` / `CreateShader` / `CreatePipelineState` 已能由反射派生
  layout/root signature 并建 PSO。
- **缺口**：没有运行时 shader 加载路径。只有测试里 inline 编译 slang → 直接建 Shader。
  没有 `AssetTraits<Shader>`、没有缓存、没有「源码 → 二进制」映射；`MaterialAssetData.shader`
  目前是一个指向「并不存在的 shader 资产」的 `Uuid`。

Shader 与普通资产不同：它是「源码 + 编译工具链 + 变体」的产物，天然带缓存语义，不适合走
`AssetTraits` 的序列化资产路径。本设计给出「相对路径身份 + 源码搜索路径 + 编译缓存」的运行时
解析链路。

模块结构（对既有 `aurora/shader` 的拆分）：

- **Shader 定义 + 解析 + 缓存** 落在 `engine/aurora/core`（target `Aurora`）；core 保持 **slang-free**。
- **编译实现** 独立为 `engine/aurora/shader`：`AuroraShaderCompiler.Static`（STATIC，slang）+ `AuroraShaderCompiler`
  （SHARED，framework 模块，经 `ModuleManager` + `modules_*.json` 加载），可按配置选择带 / 不带。
- 依据：`aurora/shader` 的公开头本就 slang-free，slang 只存在于 `ShaderCompilerSlang.cpp` 与
  `ShaderFileSystem.cpp` 的 `ISlangFileSystem` 适配段，拆分成本低。

> 关联：`aurora-shader-cache` 只负责「解析出 RHI `Shader`」。material 属性绑定、PSO 构建/缓存、
> DrawItem 组装属于 `aurora-material-pso`。

## Goals / Non-Goals

**Goals:**

- 以 `relativePath` 为 shader 身份，运行时解析出「二进制 + 反射」并创建 RHI `Shader`。
- 三级解析优先级：**离线编译缓存 > 源码现场编译 > 本地实时编译缓存**。
- 内容寻址缓存（B 方案）：同源去重、可共享、可离线预编、发布包无源码也能命中。
- 统一的变体 key 组装：pipeline 全局位 + 顶点语义 + per-shader schema，落进现有
  `ShaderVariantKey` / `ShaderVariantSchema` / `GlobalVariantLayout`。
- 建立并维护「源码 → 二进制」映射（`index.bin` 的 `artifacts`）。
- 运行时**收集 shader 使用**，为离线预编提供输入，使发布包命中率≈收集覆盖率。

**Non-Goals:**

- material → ResourceGroup 绑定、PSO 构建/缓存、DrawItem / 分桶（`aurora-material-pso`）。
- 渲染主循环 / 线程 / 命令 mailbox（`aurora-renderer`）。
- 完整变体爆炸抑制（v1 只做 128-bit 预算校验 + 「未命中即编译」）。
- 热重载 UI / 文件监听（结构上支持，交互后置）。

## Decisions

### D1. Shader 身份 = 相对路径（非 Uuid / 非 ShaderAsset）

`ShaderRef { std::string relativePath; std::string entry; }`，由 material / pass 引用；定义在
`engine/aurora/core`（与 `Material` 同模块，无需跨模块依赖）。

- **理由**：离线缓存与本地实时缓存都能按路径索引；不需要额外注册表；发布包无需携带资产元数据。
- **替代**：Uuid 资产（被否——需 ShaderAsset 序列化，与「不通过资产系统」矛盾）；
  逻辑名 + 注册表（被否——多一层 manifest 维护）。
- **注意**：路径是弱身份（重命名会失效）；用相对路径 + 搜索根保证跨机器一致。

### D2. 解析：schema / 变体 / 二进制三段

`variantHash` 由 assembler 算出（见 D5），resolver 只吃结果；schema 的鸡生蛋由独立的第一段承担。

```
ResolveSchema(path):                                   // ① schema（鸡生蛋在这步）
  if 源码旁 companion 存在          → 解析出 schema + schemaFp   // 源码优先
  else if offlineIndex.paths[p] 有效 → 用它的 schema / schemaFp  // 离线优先
  else if localIndex.paths[p] 有效   → 用它的 schema / schemaFp
  else                              → fail          // 其中 p = normalize(path)

Assemble(schema, globalLayout, vertexMask, material)   // ② 见 D5
  → variantInfo { variantKey; spec; variantHash }

ResolveShader(path, variantInfo, target):              // ③ 二进制，严格按 离线 > 源码 > 本地
  // Lookup(root) 内部先做全局失效（formatVersion/toolchainFp/layoutFp）与 schemaFp 检查
  src       = locateSource(path)                       // shaderSourceSearchPaths
  srcHash   = src ? hashIncludeClosure(src) : none
  srcUsable = src && compiler != nullptr

  // 某 root 的缓存可用性：有源码 → sourceHash 匹配；无源码 → blob 存在
  artOffline = Lookup(offlineRoot, path, variantInfo.variantHash, target)
  if artOffline && (srcHash ? artOffline.sourceHash == srcHash
                            : blobExists(offlineRoot, artOffline.compileHash)):
      return LoadBlob(offlineRoot, artOffline.compileHash)         // 【离线编译缓存】
  if srcUsable:
      return Compile(src, variantInfo) → WriteCache(local) → Load // 【源码编译】
  artLocal = Lookup(localRoot, path, variantInfo.variantHash, target)
  if artLocal && (srcHash ? artLocal.sourceHash == srcHash
                          : blobExists(localRoot, artLocal.compileHash)):
      return LoadBlob(localRoot, artLocal.compileHash)             // 【本地实时缓存】
  fail(src ? "no compiler registered (cache miss)" : "shader missing")
```

- **三段解耦**：`ResolveSchema` 只解决 schema；`Assemble` 用 schema + 渲染输入算 key；`ResolveShader`
  只解决二进制。resolver 不依赖 schema（不需要它算 hash）。
- **优先级（按 root，严格）**：`offlineRoot` > 源码现场编译 > `localRoot`。每个 root 的 `Lookup` 独立
  做全局失效与 schemaFp 检查；不再把两个 root 合并成一个 index，避免「local 优先」把 offline 顶掉。
- **全局失效**：某 root 的 `formatVersion` / `toolchainFp` / `layoutFp` 任一不符 → 该 root 整份无视，
  避免无源码分支复用旧工具链 blob。
- **root 覆盖**：schema 解析优先源码 companion；无源码时 `offline` 的 schema 优先于 `local`。
- **理由**：dev 改源码 → `sourceHash` 变 → 缓存失效 → 落源码编译（迭代新鲜）；发布包无源码 → 吃缓存。
- **副产物**：源码编译成功后写回缓存，下次成为缓存命中。
- `compiler` 取自 `ShaderCompilerFactory`：模块 `AuroraShaderCompiler` 已加载则非空，否则为空。

### D3. 缓存 = 内容寻址（B）：单 index + blobs

**磁盘布局（按 target 分目录）**

```
<cacheRoot>/                     // 可多 root：offline(只读, 随包) + local(可写)
  vulkan/  metal/  d3d12/        // 按 ShaderTarget 隔离（SPIRV/MSL/DXIL）
    index.bin                    // 版本化、层级化索引（每 target 一份）
    blobs/<compileHash>.bin      // 内容寻址、自描述；同源去重
```

- **按 target 独立目录**：`vulkan` / `metal` / `d3d12` 各自 `index.bin` + `blobs/`。
  便于**按后端打包**（发布包只带所需 target）、**按后端清理/GC**、互不干扰。
- 目录名由 `ShaderTarget` 映射（`ShaderTargetDirName`）；resolver 按 `req.target` 选取。
- `compileHash` 仍含 `target`（双保险；目录已隔离）。

**`index.bin`**

```
Index {
    u32   formatVersion                    // 文件格式版本，未知 → 整份无效
    u64   toolchainFp                      // slang/dxc 版本 + opt + codegen options
    u64   layoutFp                         // GlobalVariantLayout 指纹（P 位定义）
    map<normalizedPath, PathEntry> paths
}
PathEntry {
    u64                  schemaFp
    ShaderVariantSchema  schema            // companion 产物（鸡生蛋：独立于 blob）
    vector<string>       entryPoints       // VSMain / FSMain …
    map<(u64 variantHash, Target), Artifact> artifacts
}
Artifact {
    u128             compileHash            // → blobs/<compileHash>
    u64              sourceHash             // include 闭包
    vector<string>   sourceDeps             // 热重载 / 构建依赖
}
// layoutFp / schemaFp / toolchainFp 是"层级"字段，不进 artifact key
```

**`blobs/<compileHash>.bin`**

```
Blob {
    u32   formatVersion
    u64   sourceHash; u64 layoutFp; u64 schemaFp      // 自描述（校验 / 排查）
    {u64 words[2]; u16 totalBits} variantKey
    Target            target
    u64               moduleDigest                    // 输出 hash（命中后"可选"校验）
    ShaderReflection  reflection                      // 模块级，只存一份
    vector<StageBlob> stages
}
StageBlob { Stage stage; string entry; vector<u8> bytes }
```

- **module 级**：一个 `compileHash` 对应整个模块（所有 entry），reflection 只一份，`stages[]` 分装。
- **合并 index**：`interfaces`（schema）与 `manifest`（artifacts）合并成一个 `index.bin`；鸡生蛋只要求
  「schema 不依赖 blob」——同处一个 index 即满足。
- **键关系**：
  ```
  GlobalVariantLayout(layoutFp) ┐
  VertexSemanticMask            ├─▶ variantKey ─▶ variantHash ─┐
  schema ───────────────────────┘                              ▼
  sourceHash · layoutFp · schemaFp · target · toolchainFp ─▶ compileHash ─▶ Artifact ─▶ blobs/<compileHash>
  ```
- **失效（自顶向下）**：

  | # | 条件 | 影响 |
  |---|---|---|
  | ① | `index.formatVersion` 不符 | 整份 index 无效 |
  | ② | `index.toolchainFp` 不符 | 整份 index 无效 |
  | ③ | `index.layoutFp` 不符 | 整份 index 无效 |
  | ④ | `PathEntry.schemaFp` 不符 | 该 path 的 artifacts 全失效 |
  | ⑤ | 有源码时 `Artifact.sourceHash` 不符 | 该 artifact 失效 |
  | ⑥ | `blobs/<compileHash>` 不存在 | 该 artifact 失效 |

- **读写**：读 = `normalize(path)` → `paths[p]` → 失效检查 → `artifacts[(vh,target)]` → blob；
  写 = 先原子落 `blobs/<compileHash>`，再更新 `paths[p].artifacts`，最后原子写 `index.bin`。
- **内容寻址去重**：`blobs` key = 输入派生的 `compileHash`；同源不同路径共享 blob。
- **损坏 / 碰撞防护**：`moduleDigest` **可选**校验；`compileHash` 用 128-bit 降低碰撞。
- **路径规范化**：`normalizedPath`（`/` 分隔、大小写归一等）保证跨 OS / 机器键一致。
- **多 root → `core::MultiFileSystem`**：每个 cache root 目录实现为 `IFileSystem`，按优先级 mount 进
  `MultiFileSystem`（offline 只读 → local 可写，与 `ShaderFileSystem` 同一基类）；读取按 mount 顺序
  解析，写入定向到可写的 local 实例。**不自造 root 合并逻辑**。
- **与 D2 优先级的衔接**：`MultiFileSystem` 是文件级 fallback（offline → local）。要严格保持
  「离线 > 源码 > 本地」，resolver 分别向 offline / local 两个 mount 查询（或构造 `[offline]` /
  `[offline, local]` 两个视图），而不是一次合并读。
- **并发**：v1 单写者（render 线程）+ 目录锁；后续多进程并发可改 per-session shard index（读取时合并）。
- **驱逐**：仅 local root 需要容量上限 + LRU（**v1 预留，见 Risks**）；offline 只读不驱逐。
- **对齐既有抽象**：扩展 `ShaderVariant.h` 的 `ShaderCacheKey`（加 `layoutFp` / `schemaFp` / `toolchainFp`），
  并把本存储实现为 `ShaderCache` 接口的后端（offline 只读 / local 可写两实例）；不再另起一套 key。

### D4. 编译键与 sourceHash

```
compileHash = H(sourceHash, layoutFp, schemaFp, variantHash, target, stage, entryHash, toolchainFp)  // entry 级
sourceHash  = hash(include 闭包: 根文件 + 所有被 include 的文件 + companion)
toolchainFp = slangVer + dxcVer + optLevel + codegenOptionsHash
```

- **entry 级（v1）**：编译器一次产出一个 entry 的 `data` + reflection（`ShaderCompileDesc` 单 `stage`+`entry`），
  故 `compileHash` 含 `stage` + `entryHash`，各 entry 独立存 blob，互不覆盖。
- **module 级（后续）**：一次编译产出模块全部 entry、reflection 只存一份；届时可从 key 去掉 `stage`/`entry`，
  用 `StageBlob[]` 分装。v1 不做。
- **对应 `ShaderCacheKey`**：`compileHash` 即扩展后 `ShaderCacheKey` 的内容（`sourceHash` / `variantHash` /
  `target` / `stage` / `entryHash` + `layoutFp` / `schemaFp` / `toolchainFp`）的摘要。
- **理由**：工具链变化会改变产物，必须进 key，否则旧 blob 被错误复用。
- `sourceDeps[]` 落进 `Artifact`（热重载 + 构建依赖）。
- v1 实现：优先使用 slang 依赖上报；若不可用，退化为「根文件 hash + companion 中显式声明的
  `depends`」。**递归 `#include` 扫描**作为后备。

### D5. 变体 key 组装

**位图分区（必须版本化）：**

```
bit 0            ┌─────────────────────────────┐
                 │ pipeline 保留区 (GlobalLayout)│  P bits  ← renderer/pass
 bit P            ├─────────────────────────────┤
                 │ vertex semantics 区           │  V bits  ← 几何顶点流
 bit P+V          ├─────────────────────────────┤
                 │ per-shader schema 区          │  S bits  ← schema
 bit P+V+S        └─────────────────────────────┘   约束 P+V+S <= 128
```

- `SetPipelineBit` 用绝对偏移；`SetVertexSemantics(P, mask)`；schema 条目用相对偏移，
  `EntryAbsoluteOffset = P + V + entry.bitOffset`（现有接口已留此语义）。
- `GlobalVariantLayout` 增加 `fingerprint`（即 index 的 `layoutFp`）并进 `compileHash`：位定义变更必须失效。

**贡献者：**

| 区 | 谁定值 | 时机 |
|---|---|---|
| pipeline | renderer/pass（HDR、MSAA、阴影质量、debug view…） | 每帧/每 pass |
| vertex semantics | 几何顶点流 `VertexSemanticMask` | 收集 DrawItem 时 |
| per-shader schema | ① schema 声明 ② material 覆盖 ③ 未覆盖取 default | material 解析 |
| specialization | schema 中 `isSpec=true` 的值 | 同上 |

**组装算法：**

```
BuildVariantKey(inputs) -> { ShaderVariantKey key; ShaderSpecialization spec; uint64 variantHash; }
  1. key.totalBits = P + V + S
  2. pipeline 位 → SetPipelineBit
  3. 几何 mask → SetVertexSemantics(P, mask)
  4. schema entries: v = material.override(key) ?: default
       isSpec ? spec.entries += {specId, v} : key.Set(schema, key, v)
  5. variantHash = key.ContentHash()
```

**schema 载体 = companion `.slang.json`：**

- 与源码同目录、同生命周期，`relativePath + ".json"`；声明 sources/entries/位宽/default/isSpec/specId。
- 算入 `sourceHash`（改 schema 必须失效）。
- 离线 cook 把它并入 `index.bin` 的 `PathEntry.schema`。
- **替代**：纯 slang 反射（被否——软开关不在反射里）；内联 slang 属性（成本高，后置）。
- `ShaderVariantAssembler` 依赖 `GlobalVariantLayout`（`aurora/pipeline`），落在 **pipeline / renderer**
  侧，不放 core，避免 core → pipeline 反向依赖。
- schema 解析同样遵循三级优先级：**源码旁 companion > 离线 `index.bin`**；都没有则报错。

### D6. spec constant：v1 统一编进 moduleKey

spec 值直接进 `variantHash`（即进 `compileHash`），不做 moduleKey / psoKey 分层。

- **理由**：Vulkan 下 spec 只改 PSO 不改 SPIRV，分层能去重；但实现与缓存格式复杂。v1 先简单，
  Vulkan 少一点去重可接受。
- **后续**：拆 `moduleKey = H(sourceHash, softVariantHash, target, toolchain)`（仍 module 级，不含 entry）与
  `psoKey = H(moduleHash, specValues, vertexLayout, format, state)`。

### D7. 运行时创建与线程

- `blob + reflection → Device::CreateShaderFunction / CreateShader`（在 **render 线程**，因为 Device
  confined）。
- slang 编译是 CPU 重活 → 建议丢 `ThreadPool` worker，编译结果回 render 线程建对象，避免卡帧。
- `TechniqueCache`（C3）以 `ShaderRef` + `variantHash` 为键，不重复建 Shader。

### D8. 代码落点

```
engine/aurora/core     (target Aurora)                      ← Shader 定义 + 解析 + 缓存（slang-free）
  ShaderRef, ShaderVariant / Schema / Key
  RgBlockDesc, ShaderFileSystem（搜索路径 / 虚拟文件部分）
  IShaderCompiler（抽象）+ ShaderCompilerFactory（注册）
  ShaderResolver + ShaderCache（index / blobs；多 root 经 core::MultiFileSystem）

engine/aurora/shader    (target AuroraShaderCompiler.Static) ← 编译实现（STATIC，slang）
  ShaderCompilerSlang.cpp, SlangFileSystemAdapter.cpp
  gen/ShaderCodeGen.cpp, gen/ShaderBlockGen.cpp
  依赖 Aurora(core) + Aurora.RHI + slang/dxcompiler
                        (target AuroraShaderCompiler)       ← framework 模块（SHARED）
  module/AuroraShaderCompilerModule.cpp：Init 注册 Static 实现
```

- **static / shared 拆分**（对齐 `AuroraCook.Static` + `Aurora.Cook`）：host 工具与离线 cook 链
  `AuroraShaderCompiler.Static`；运行时 framework 模块（SHARED）链 Static 并注册进 factory。
- `ShaderVariantAssembler` 依赖 `GlobalVariantLayout`（pipeline），落在 **pipeline / renderer** 侧，
  不放 core，避免 core → pipeline 反向依赖。
- `Shader` 的 RHI 对象（`aurora/rhi/Shader.h`）仍在 RHI 层；core 已 link `Aurora.RHI`。

### D9. 编译器为可选 framework 模块（可不带，也可以带）

- `AuroraShaderCompiler`（SHARED，`IModule`）经 `ModuleManager` + `modules_*.json` 加载；
  `Init` 时向 core 的 `ShaderCompilerFactory` 注册 `ShaderCompilerSlang`。
- **可不带，也可以带**——两种都是合法发布形态，按项目取舍，不是硬绑定：
  - **带上**（dev / editor，或需要兜底的发布构建）：源码可现场编译；缓存 miss 时**兜底**，不会因
    收集不全而失败。
  - **不带**（追求体积 / 启动 / 不暴露编译器）：factory 为空 → `ShaderResolver` 只走 offline / local
    cache；miss 即明确报错（可靠性由 D10 的收集覆盖度保证）。
- 离线产出：`ShaderHeaderTool`（host）产出 `*.gen.h` / `ShaderHeaders`；**离线 shader cook**（host，链
  `AuroraShaderCompiler.Static`）读 `shader_usage.json` 产出 runtime 用的 `index.bin` / `blobs`。

### D10. Shader 使用收集（供离线预编）

离线缓存要「恰好」包含发布包会用到的 shader，需要一个**运行时收集 → 离线预编**的闭环：

```
运行时 resolve（hit 或 miss 都记）
   → 内存去重集合 ShaderUsageEntry { relativePath; entry; stage; target;
                                     variantKey; variantHash; variantDump; toolchainFp; }
   → Shutdown / 显式 Flush 时 union 合并写入 shader_usage.json
离线工具
   → 读 shader_usage.json：按 (relativePath, target, variantHash) 聚合 entry（取并集）
   → 对每个模块复用同一 compile（无 Device，仅编译）→ 产出 shipped index.bin / blobs
```

- **去重键**：`(relativePath, entry, target, variantHash)`；跨会话 **union 合并**（append + 去重），可多次收集累积。
- **entry 级收集 / module 级编译**：usage 按 entry 记录，离线按模块聚合后一次编译该模块的 entry **并集**。
- **开关**：`collectShaderUsage`（dev / editor 默认开；发布默认关），避免运行时无谓 IO。
- **收集时机**：所有 resolve（含缓存命中），反映「实际用到」的集合；`variantDump` 保留可读键值便于排查。
- **离线复用同一编译路径**：`AuroraShaderCompiler.Static` 的 host 形态直接喂 usage，避免运行时/离线两套逻辑漂移。
- **局限**：只覆盖被跑到的变体；未覆盖变体在「不带编译器」的发布包里会 miss——此时可**带上编译器兜底**
  （见 D9）。PSO 级状态的收集属 `aurora-material-pso`。

### D11. 本地写：专用 IO 线程 + 内存索引快照 + 原子替换

- `ShaderCacheWriter`（core）持一根**专用 IO 线程**（不占 render、不占 worker），独占本地索引与全部落盘：
  - 生产者（worker / render）经 `Submit(Pending)` 入队（加锁队列，非阻塞）。
  - IO 线程 drain 批 → 写 blob（原子）→ 更新内存 index → 原子写 `index` → 发布只读**快照**。
  - `Flush()` 阻塞至 drain + 落盘；`Stop()` 收尾并 join。
- 读者（resolver）经 `LookupSchema` / `LookupArtifact` 读快照（`shared_ptr<const ShaderCacheIndex>`，短锁），不再每次读盘。
- **消除读改写竞态**：索引只由 IO 线程写（单写者）→ 无丢条目。
- **原子替换**：`WriteFileAtomic`（临时文件 + `IFileSystem::Rename`）→ 读者不会看到半截文件；不支持 `rename` 的文件系统退化为直写。
- **同步回退**：未设 writer 时 resolver 走同步路径，用 `UpsertLocalArtifact`（进程 `recursive_mutex` + 目录锁罩住整段 RMW）→ 同样不丢条目。
- **崩溃一致性**：进程崩溃最多丢最近未落盘的若干条索引（blob 已写 → 孤儿，可 GC）。可接受。

### D12. 编译任务独立：内联（render parallel）/ 异步（worker）二选一

- `ShaderCompileTask`（core）是**自包含编译载荷**（own `source` / `schema`，`variant` 指向 material），
  提供 `MakeDesc()` / `Run(compiler, out)`，可在任意线程执行。
- `ShaderResolver::PrepareShader` 只做到「缓存命中 or 生成 task」，**不做线程决策**：
  - `Ready`：缓存命中，直接返回；`Compile`：返回可运行 task；`Failed`：无缓存且无法编译。
- `ShaderCompileFuture` 包 task + compiler + commit：`RunInline()`（当帧 render parallel，阻塞不可跳过）
  或 `RunAsync(pool)`（worker，`Ready()` 前可跳过）；完成时自动 `CommitCompile` → writer 队列。
- `ResolveShader`（同步便捷）= Prepare → `RunInline` → Commit。
- 编译产物回 **render 线程建 RHI `Shader`** 由 C1 编排（Device 独占）。

## Risks / Trade-offs

- [变体爆炸] → 预算校验 `P+V+S<=128`；限制单 shader 软开关数；优先 pipeline 位收敛；不追求全组合预编。
- [路径身份脆弱，重命名失效] → 相对路径 + 搜索根；可在 companion 记稳定 id 供后续加注册表。
- [include 闭包 hash 不准 → 改了依赖不失效] → 依赖上报优先；退化为声明 `depends` + 自扫描；`sourceDeps` 落盘可审计。
- [schema 与源码不一致（companion 漂移）] → companion 计入 `sourceHash`；spec 部分与 slang 反射交叉校验。
- [旧 blob 被误复用] → `compileHash` 含 `toolchainFp` / `layoutFp` / `schemaFp`；index 再做层级失效兜底。
- [编译卡帧] → worker 编译 + render 线程建对象；首帧 miss 允许同步编译（可接受）。
- [本地缓存并发写] → v1 单写者（render 线程）+ 临时文件 + 原子 rename + 目录锁；后续多进程改 shard index。
- [多 target] → 每 target 独立 blob；按当前后端 API 选择，切后端自然重新解析。
- [未加载编译器模块却需要源码编译] → factory 为空即明确报错；发布包依赖 offline / local cache。
- [framework 模块加载时序] → 注册发生在模块 `Init`；注册前调用 resolver 视为「不带编译器」。
- [编译器输出非确定 → 同 key 不同字节] → 首个写入胜；可选对输出做 hash 校验。
- [缓存无驱逐策略 → 无限增长] → v1 不做容量控制，后续加 LRU / 清理。
- [使用收集不全 → 发布包 miss] → 收集跑代表性流程；关键 shader 支持显式声明必需变体；或该发布构建
  **带上 `AuroraShaderCompiler` 作 miss 兜底**；不带时 cache-only 仍明确报错。

## Migration Plan

1. 把 `aurora/shader` 的 slang-free 部分（variant / gen 解析 / RgBlockDesc / ShaderFileSystem 非 slang 段）
   并入 `engine/aurora/core`；新增 `IShaderCompiler` / `ShaderCompilerFactory`。
2. 把现有 `Aurora.Shader` 拆为 `AuroraShaderCompiler.Static`（slang 实现）+ `AuroraShaderCompiler`
   （framework 模块包装），迁入 `ShaderCompilerSlang.cpp` + slang FS 适配并注册进 factory；`modules_*.json` 按需列出。
3. 消费者重指向：`pipeline`（`ShaderVariant` / `gen` 解析 / `RgBlockDesc`）改链 `Aurora`(core)；
   `ui/render`（运行时编译）改经 `IShaderCompiler` / 模块；`ShaderHeaderTool` 链 `AuroraShaderCompiler.Static`。
4. `ShaderResolver` / `ShaderCache` 落在 core（新增，不破坏现有）。
5. 离线 cook 增加导出 `index / blobs`（格式与本地实时一致）。
6. material / adaptor 的 shader 引用由 `Uuid` 迁移为 `ShaderRef`（与 `aurora-material-pso` 一起落）。
7. 回滚：解析器与缓存均为新增；编译器模块可按配置摘除，不影响 cache-only 路径。

## Open Questions

- `GlobalVariantLayout` 的 P / V / S 具体位宽分配（V 是否固定 32）与版本化方案。
- include 闭包 hash 的实现选择（slang 依赖 API vs 自扫描）与 v1 深度。
- 编译放 worker 线程的生命周期/取消策略；首帧同步编译是否接受。
- companion `.slang.json` 与 slang 反射在 spec 部分的权威性（谁覆盖谁）。
- `index.bin` / `blobs` 的二进制 schema 版本号策略。
- `AuroraShaderCompiler` 的模块名 / config 键名，以及与 `AuroraRender` 的加载依赖关系。
- 源码编译失败是否回退本地缓存的「最后一次好版本」（dev 体验）——会改变 D2 优先级语义。
- `shader_usage` 文件位置 / 格式（JSON vs bin）与跨机器合并策略。
- 是否同时收集 PSO 级状态（归 `aurora-material-pso`）。
