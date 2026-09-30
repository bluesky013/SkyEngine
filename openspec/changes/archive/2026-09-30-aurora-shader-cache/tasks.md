## 1. 模块拆分（core / compiler）

- [x] 1.1 把 `aurora/shader` 的 slang-free 部分迁入 `engine/aurora/core`：`ShaderVariant` / `ShaderVariantSchema` / `ShaderVariantKey` / `BuildVertexVariant`、`RgBlockDesc`、`gen/ShaderVariantGen` / `gen/ShaderCodeGen` 解析、`ShaderFileSystem`（搜索路径 / 虚拟文件段，slang 适配拆出）
- [x] 1.2 `engine/aurora/core` 保持 slang-free：确认新增代码无 `#include slang`，CMake 不链 slang / dxcompiler
- [x] 1.3 新增 `ShaderRef { relativePath, entry }` 与路径归一化（`normalize`）于 `engine/aurora/core`
- [x] 1.4 新增抽象 `IShaderCompiler` 与 `ShaderCompilerFactory`（注册 / 查询），定义于 `engine/aurora/core`
- [x] 1.5 消费者重指向：`pipeline` 改链 `Aurora`(core)；`ui/render` 改经 `IShaderCompiler`；`ShaderHeaderTool` 移出 core 依赖
- [x] 1.6 构建通过（interface / core / pipeline / ui-render）

## 2. 编译器模块（AuroraShaderCompiler）

- [x] 2.1 新建 `AuroraShaderCompiler.Static`（STATIC）：迁入 `ShaderCompilerSlang`（实现 `IShaderCompiler`）、`SlangFileSystemAdapter`、`gen/ShaderCodeGen` / `gen/ShaderBlockGen`；链 `Aurora`(core) + `Aurora.RHI` + slang / dxcompiler
- [x] 2.2 新建 `AuroraShaderCompiler`（SHARED，`IModule`）：`Init` 向 `ShaderCompilerFactory` 注册 `ShaderCompilerSlang`
- [x] 2.3 `configs/modules_*.json` 按需列出该模块（dev / editor 默认带）
- [x] 2.4 `ShaderHeaderTool` 链 `AuroraShaderCompiler.Static`；离线 shader cook（host）链 `.Static`，读 usage 产出 `index` / `blobs`
- [x] 2.5 构建 + 现有 `AuroraShaderTest` 全绿

## 3. 变体 key 组装

- [x] 3.1 `GlobalVariantLayout` 增加 `fingerprint`；定义 `P | V | S` 位布局并做 `P+V+S <= 128` 校验
- [x] 3.2 新增 `ShaderVariantAssembler`（pipeline 位 + 顶点语义 + schema + material 覆盖 / default → `variantInfo{variantKey, spec, variantHash}`）；落在 pipeline / renderer 侧
- [x] 3.3 v1 把 spec 值并入 `variantHash`（不做 moduleKey / psoKey 分层）
- [x] 3.4 测试：顶点语义位、material 覆盖与默认、`P+V+S > 128` 报错

## 4. schema companion（`.slang.json`）

- [x] 4.1 companion 解析：声明 sources / entries / 位宽 / default / isSpec / specId，产出 `schema` + `schemaFp`
- [x] 4.2 companion 计入 `sourceHash`；schema 解析优先级 源码 companion > 离线 index
- [x] 4.3 测试：companion 优先、无 companion 回退 index、schemaFp 变化失效

## 5. 缓存存储（index + blobs）

- [x] 5.1 扩展 `ShaderCacheKey` 为 `{sourceHash, variantHash, target, layoutFp, schemaFp, toolchainFp}`
- [x] 5.2 `index.bin` 读写：`formatVersion` / `toolchainFp` / `layoutFp` + `map<normalizedPath, PathEntry{schemaFp, schema, entryPoints, artifacts}>`；原子写 + 版本号
- [x] 5.3 `blobs/<compileHash>` 读写：内容寻址、自描述（reflection + `moduleDigest` + `StageBlob{stage, entry, bytes}[]`）
- [x] 5.4 多 root 经 `core::MultiFileSystem`：offline / local 目录各实现 `IFileSystem` 并按优先级 mount；全局失效（formatVersion / toolchainFp / layoutFp）+ 层级失效（schemaFp）
- [x] 5.5 实现 `ShaderCache` 接口后端：读取经 `MultiFileSystem`（必要时分别查 offline / local 两 mount），写入定向 local；单写者 + 目录锁
- [x] 5.6 测试：层级失效、内容寻址去重、多 root 顺序（offline 优先）

## 6. Resolver

- [x] 6.1 `ResolveSchema`：源码 companion > offline index > local index
- [x] 6.2 `ResolveShader`：offline > 源码现场编译 > local；有源码要求 `sourceHash` 匹配、无源码要求 blob 存在；无编译器且 miss 明确报错
- [x] 6.3 `sourceHash` 的 include 闭包：优先 slang 依赖上报，退化为「根文件 + companion 声明 depends + 自扫描」
- [x] 6.4 测试：离线命中 / 源码变更触发编译 / 无源码命中本地 / 无编译器 miss 报错

## 7. 运行时创建

- [x] 7.1 blob + reflection → `Device::CreateShaderFunction` / `CreateShader`（render 线程）
- [x] 7.2 编译（slang）建议丢 worker，结果回 render 线程建对象（避免卡帧）
- [x] 7.3 预留 `TechniqueCache` 键（`ShaderRef` + `variantHash`）供 `aurora-material-pso` 复用

## 8. 使用收集（离线预编）

- [x] 8.1 `ShaderUsageCollector`：内存去重集 `ShaderUsageEntry`，开关 `collectShaderUsage`（dev / editor 默认开）
- [x] 8.2 Flush：跨会话 union 合并写入 `shader_usage.json`
- [x] 8.3 离线聚合：按 `(relativePath, target, variantHash)` 聚合 entry 并集，按 module 编译产出 `index` / `blobs`
- [x] 8.4 测试：记录、union 合并、离线按模块聚合

## 9. 验证与收尾

- [x] 9.1 扩展 `AuroraShaderTest`（resolver / cache / variant assembler / usage）
- [x] 9.2 构建 + `ctest` 全绿
- [x] 9.3 运行 clang-format / clang-tidy 策略，确认符合规范
- [ ] 9.4 待用户确认后 `openspec archive aurora-shader-cache`
