## Why

当前 `ShaderCompilerSlang::Compile(ShaderCompileDesc)` 每次编译产出「单一、无变体」的 shader：没有 key→value 变体模型，强变体（`#define` 语言裁剪）与弱变体（specialization constant）都无处表达，也无 cache key。材质/特性开关只能靠调用方拼 `#define` 字符串。本 change 设计 aurora.shader 的变体接口——统一 `Name→value` 模型（接口/cache key 不区分强/弱），bitmask key + 数据驱动 schema，多来源（pipeline/vertex/batch）位组合成最终 cache key，机制由 shader 声明决定、后端处理，并预留离线 shader cache。

## What Changes

- **统一变体模型**：`ShaderVariantEntry{Name key, uint32 value}` + `ShaderVariant`，无强/弱类型。
- **bitmask key**：`ShaderVariantKey`（128bit + 越界校验），多来源不相交位 `operator|=` 组合。
- **数据驱动 schema**：`ShaderVariantSchema`（source = `Name` tag，位布局/宽度全数据驱动）；`GlobalVariantLayout` 全局预留 pipeline 位。
- **机制由 shader 声明决定**：编译层反射解析 name → spec 常量（SPIRV/MSL native specialize、DXIL 折叠）或宏（`-D` 折叠）。
- **多来源组合**：`pipeline`（PipelinePass）/ `vertex`（mesh∩用户设置）/ `batch`（材质）三段位 OR 成最终 key。
- **cache 统一 key**：`ShaderCacheKey{sourceHash, variantHash, target}` + 预留 `ShaderCache` 接口（Load/Store，本 change 只定义）。
- **数据驱动落地**：`ShaderHeaderTool` 扩展解析 shader 内 `@variant` 注释块 + `[SpecializationConstant]` 反射 → 生成 `.variant.h`（离线 codegen），外部 shader 走运行时 JSON（后续）。

## Capabilities

### New Capabilities

- `aurora-shader-variant`: aurora.shader 变体接口——统一 `Name→value` 模型、128bit bitmask key、数据驱动 schema、多来源组合、机制后端处理、统一 cache key 与离线 `ShaderCache` 接口预留。

## Impact

- **engine/aurora/shader**：新增 `ShaderVariant.h`（`ShaderVariantEntry`/`ShaderVariant`/`ShaderVariantKey`/`ShaderVariantSchema`/`GlobalVariantLayout`/`ShaderCache`/`ShaderCacheKey`）；`ShaderCompileDesc` 增加 `variant` + `cache`；`Compile` 解析 name→宏/spec；`ShaderHeaderTool` 扩展生成 `.variant.h`。
- **engine/aurora/rhi**：不改（变体在编译层解析，RHI 拿最终二进制；`ShaderReflection` 复用）。
- **范围边界**：`ShaderCache` 只定义接口不实现；资源绑定（`aurora-resource-group`）不实现，仅确立 heap+offset 抽象方向；运行时 JSON 通道后续。
