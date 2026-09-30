## Why

RHI 已能由 shader 反射派生布局并创建 PSO，但运行时**没有 shader 的加载路径**：只有测试里
inline 编译 slang 后调 `Device::CreateShaderFunction` / `CreateShader`；没有缓存、没有
「源码 → 二进制」映射。Shader 属于特殊资源（不注册 `AssetTraits`），需要一条
「源码搜索路径 + 编译缓存」的解析链路，并让离线编译产物优先。

## What Changes

- 新增 `ShaderRef { relativePath, entry }`：以**相对路径**为 shader 身份，供 material / pass 引用
  （取代 `Uuid` shader 引用）。
- 新增 `ShaderResolver`：解析顺序 **离线缓存 > 源码现场编译 > 本地实时编译缓存**；全部 miss 报错。
- 新增内容寻址 shader cache：
  - `index.bin`（版本化、层级化）：`layoutFp` + `map<normalizedPath, { schemaFp, schema, entryPoints,
    artifacts }>`；`artifacts`: `(variantHash, target) → { compileHash, sourceHash, sourceDeps[] }`，
    即**源码 → 二进制映射**（schema 与 blob 解耦，解决「算 key 需要 schema、找 blob 又需要 key」的鸡生蛋）。
  - `blobs/<compileHash>`（内容寻址、自描述）：`{ reflection; moduleDigest; StageBlob{stage, entry, bytes}[] }`。
- `compileHash = H(sourceHash, layoutFp, schemaFp, variantHash, target, stage, entryHash, toolchain)`（**entry 级**）；
  `sourceHash` 覆盖 **include 闭包 + companion**。
- 新增 `ShaderVariantAssembler`：组装 128-bit variant key（pipeline 保留区 + 顶点语义区 + per-shader
  schema 区）+ `ShaderSpecialization`。
- schema 载体：companion `.slang.json`（与源码同生命周期，也算入 `sourceHash`）。
- v1：spec constant 的值**统一编进 moduleKey**（暂不做 moduleKey / psoKey 分层）。
- 运行时：blob + reflection → `Device::CreateShader*`（在 render 线程创建）。
- 模块结构：Shader **定义 / 解析 / 缓存** 落 `engine/aurora/core`（slang-free）；**编译实现** 为
  `AuroraShaderCompiler.Static`（STATIC，slang）+ `AuroraShaderCompiler`（SHARED，framework 模块包装，
  经 `ModuleManager` 加载）——**可不带**（只吃离线 / 收集缓存，miss 报错），**也可以带**（缓存 miss 时
  现场编译兜底）。
- 运行时 **shader 使用收集**（`collectShaderUsage`，dev/editor 默认开）：记录每次 resolve 的
  `(relativePath, entry, target, variantKey)`，union 合并写入 `shader_usage.json`；离线工具读它复用同一
  编译路径产出 shipped 缓存，实现「离线预编」闭环。

## Capabilities

### New Capabilities

- `aurora-shader-cache`: shader 解析 / 编译缓存 / 变体 key 组装 / 源码到二进制映射 / 使用收集（离线预编）。

### Modified Capabilities

- `aurora-shader`: variant schema 的载体与 resolver 对接；编译产物进入内容寻址缓存与离线产物格式。

## Non-goals

- material 属性 → ResourceGroup、PSO 构建/缓存、DrawItem / 分桶（见 change `aurora-material-pso`）。
- 渲染主循环 / 线程 / 命令 mailbox（见 change `aurora-renderer`）。
- 变体爆炸的完整抑制策略（v1 只做预算校验与「未命中即编译」）。

## Impact

- 拆分 `engine/aurora/shader`：slang-free 的定义 / 解析 / 缓存并入 `engine/aurora/core`（target `Aurora`）；
  编译实现成为 framework 模块 `engine/aurora/shader`（target `AuroraShaderCompiler`，SHARED）。
- material / adaptor 的 shader 引用从 `Uuid` 改为 `ShaderRef`（与 `aurora-material-pso` 联动）。
- 离线 cook 需产出 `index.bin` / `blobs`（format 与本地实时缓存一致）。

## Open Questions

- 编译放 worker 线程 vs render 线程（slang 编译为 CPU 重活）。
- include 闭包 hash：slang 依赖 API vs 自扫描 `#include`；v1 是否先用「根文件 + 显式声明依赖」。
- `GlobalVariantLayout` 的 P / V / S 位宽分配与版本化（fingerprint 进 compileHash）。
