## Context

现状（`dev_refactor_rhi`）：

- `RgBlockDesc` 是 C++ 侧单一事实源（`aurora-resource-tiers`）：`{set, binding, blockName, kind, fields[]}` 产出 RHI `ResourceGroupLayout` 与 HLSL 文本（`ShaderBlockGen`）。
- C++ 侧镜像 struct（`GlobalParams`）是手写的，注释明确要求「与生成的 cbuffer 布局一致」。
- shader 侧没有真实 `.slang` 资产；`ShaderCompilerSlang` 只编译测试里的内联字符串，反射已收敛到 `ShaderReflection`（`resources` + `blocks`，block 含成员 name/offset/size）。
- `ShaderBlockGen.h` 注释预留了 `ShaderFileSystem`（virtual include）但未实现；无离线 codegen 工具，无构建集成。

约束：三层 RG（Global set0 / Pass set1 / Batch set2）；三平台 set→layout 映射约定；slang 反射是布局权威（不跨平台反射对方产物）。

## Goals / Non-Goals

**Goals:**

- 让 shader（`.slang`）成为资源块布局的单一事实源；C++ struct 与 `RgBlockDesc` 从反射派生，机械消除漂移。
- 离线 codegen host 工具产出两侧头文件，接入 CMake 构建。
- `ShaderCompilerSlang` 支持虚拟 include，shader 可 `#include` 共享/生成头。
- 三方（生成 C++ struct、shader 反射、`RgBlockDesc`）一致性校验纳入测试。

**Non-Goals:**

- 不改三层 RG 语义与三平台映射约定。
- 不做独立 IDL/DSL（Slang 已是语言）。
- 不迁移现有 HLSL 资产，不接 runtime 主渲染循环。
- 不做跨语言通用反射框架，只服务引擎 shader。

## Decisions

### 1. 事实源放在 Slang `.slang` 侧

作者在 `.slang` 中声明一次 struct + `ParameterBlock` + 绑定，C++ 全由反射派生。**备选**（C++ `RgFieldType` 封闭枚举为源）被否：枚举永远追不上 shader 类型系统（嵌套 struct、数组、`uint/int/bool`、`RWStructuredBuffer`），且仍要手写镜像 struct。

### 2. 反射驱动 codegen，产出 C++ 头

新增 host 工具（复用 `ShaderCompilerSlang` 的 program layout 反射，不重造解析器），对每个 block 产出：

```cpp
// generated: GlobalBlock.gen.h
struct GlobalParams {          // 字段类型来自映射表；alignas 对齐 std140
    Matrix4 view; Matrix4 proj; Matrix4 viewProj; Vector4 cameraPos;
};
static_assert(sizeof(GlobalParams) == 128);
static_assert(offsetof(GlobalParams, cameraPos) == 64);

inline const RgBlockDesc& GetGlobalBlockDesc(); // 由反射生成的 set/binding/kind/fields
```

关键：**offset/size 来自 slang 反射**（非 `ComputeFieldOffsets` 重算），C++ struct 用 `alignas(16)` 强制 std140 对齐（`vec3` 占 16B），并带 `static_assert(sizeof/offsetof)` 编译期把关。**备选**（用 `ComputeFieldOffsets` 计算）被否：那是 C++ 侧重新推导布局，等于又回到「两侧各算一遍」。

### 3. Slang 类型 → C++ 类型映射表

有限、稳定的映射表（`ShaderTypeMap`）：

| slang 类型 | C++ 类型 |
|---|---|
| `float` / `float2` / `float3` / `float4` | `float` / `Vector2` / `Vector3` / `Vector4` |
| `float4x4` | `Matrix4` |
| `uint`/`uint32_t`、`int`/`int32_t` | `uint32_t` / `int32_t` |
| `bool` | `uint32_t`（HLSL bool 占 4B，避免 C++ `bool` 尺寸歧义） |
| 定长数组 `T[N]` | `T[N]`（递归映射） |
| 嵌套 struct | 递归生成嵌套 C++ struct |
| texture/sampler/buffer | 不进 struct，作为资源字段进 `RgBlockDesc` |

`RgFieldType` 相应扩展 `UINT/INT/BOOL`（并保留资源类型），或字段承载「slang 类型名」字符串以支持任意类型（反射校验用）。

### 4. 虚拟 include：`ShaderFileSystem`（`ISlangFileSystem`）

Slang `ISession` 可注入自定义文件系统回调（`loadFile` → `ISlangBlob`）。`ShaderFileSystem` 把内存中生成的共享头作为 virtual include 提供给 `loadModuleFromSourceString`，`#include "Generated/GlobalBlock.slang"` 直接命中，**不落盘**。生成物亦可写 build tree 供 C++ include（同一份生成逻辑，两种消费）。确切的 slang 文件系统接口名（`ISlangFileSystem` / `ISlangFileSystemExt`）在实现时按 pinned slang 版本确认。

### 5. set/binding 权威来自 `[[vk::binding(b, set)]]` 注解

`.slang` 的 `ParameterBlock` 声明带显式 `[[vk::binding(0, 0)]]`，反射读出 set/binding 作为 `RgBlockDesc` 的 `set/binding`（Global=0/Pass=1/Batch=2 由注解固定，而非声明序自动分配）。若 pinned slang 对 `ParameterBlock` 不尊重该注解，则退化为「声明序 + 显式 `__register_space`」约定（列为 Open Question）。

### 6. 一致性校验：编译期 `static_assert` + 运行时 `ReflectionValidation`

- 编译期：生成 struct 自带 `static_assert(sizeof/offsetof)`，布局一旦漂移直接编译失败。
- 运行时：复用 `ReflectionValidation`，把生成的 `RgBlockDesc` 与运行时 shader 反射比对（set/binding/字段顺序/类型/offset），兜底非编译期可捕获的偏差。

### 7. 构建集成：CMake 自定义命令

host codegen 工具（`ShaderHeaderTool`，链接 `Aurora.Shader` + slang）作为 CMake `add_custom_command`，输入 `.slang` → 输出 `*.gen.h` / `*.gen.slang` 到 build tree；`Aurora.Pipeline` 把 gen 目录加入 include path。生成物用 `OUTPUT`/`DEPENDS` 保证增量化；ContentHash（现有 `ShaderBlockGen::ContentHash` 思想）作为 cache key。

### 8. `RgBlockDesc` 角色收敛

`RgBlockDesc` 保留为运行时布局桥（`set/binding/kind/stages` → `ResourceGroupLayout`），但不再手写：由 codegen 反射生成。其 `fields[]` 保留用于反射校验与资源字段登记；C++ 侧布局以生成 struct 为准，`ComputeFieldOffsets` 不再作为 C++ 布局权威（可保留用于 debug 对照）。

## Risks / Trade-offs

- **[slang 对 `ParameterBlock` 的 `[[vk::binding]]` 尊重度不确定]** → 实现时先验证；不满足则用 `__register_space` 或显式偏移约定（Open Question）。
- **[C++ struct 与 std140 对齐陷阱]**（`vec3`=16B、`bool`=4B、数组 stride）→ 生成 struct 逐字段 `alignas` + `static_assert(offsetof)` 把关；映射表覆盖这些特例。
- **[构建期依赖 slang 反射]**（生成 C++ 头需先跑 Slang）→ codegen 工具与 shader 目标解耦，生成物缓存（ContentHash）避免每次重跑；slang 离线已可运行（spike 验证）。
- **[类型映射表覆盖不全]**（`half`、`RWStructuredBuffer`、MSAA texture）→ 首版映射表覆盖引擎现有 UBO 需求，未覆盖类型编译期报错（不静默降级），后续按需扩展。
- **[slang COM 生命周期脆弱]**（spike 期泄漏）→ codegen 工具为一次性短命进程，泄漏影响可控；转正时一并处理。

## Migration Plan

1. 落地 `ShaderFileSystem`（virtual include）+ `ShaderCompilerSlang` 接入 include 解析。
2. 新增 `ShaderTypeMap`（slang → C++ 类型映射）+ 反射发射 C++ struct 的 codegen 逻辑。
3. 新增 `ShaderHeaderTool`（host 工具）+ CMake 自定义命令，输出 gen 头到 build tree。
4. 把 `GlobalRenderResources` 的 `GlobalParams`/`GetGlobalBlockDesc()` 切到生成头，删除手写镜像。
5. 引入首个 `.slang` 引擎 shader（`#include` 生成头），补 codegen 一致性测试。
6. 待用户确认后 `openspec archive aurora-shader-header-codegen`。

## Open Questions

- pinned slang 版本对 `ParameterBlock` 上 `[[vk::binding(b, set)]]` 的实际语义？（决定 Decision 5 的主/备方案）
- `RgFieldType` 是扩展新标量类型，还是改为携带 slang 类型名字符串？（影响反射校验精度）
- host codegen 工具是独立 exe 还是复用现有 `ShaderTool` 入口？
