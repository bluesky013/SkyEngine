# aurora-shader-variant Specification

## Purpose
TBD - created by archiving change aurora-shader-variant. Update Purpose after archive.
## Requirements
### Requirement: 统一变体模型

aurora.shader SHALL 提供统一 `Name→value` 变体模型，接口与 cache key 不区分强/弱：

- `ShaderVariantEntry{Name key, uint32 value}` — key 与 shader 里宏名/spec 常量名一对一
- `ShaderVariant` — 一次具体变体选择（entries 列表）

#### Scenario: 统一表达

- **WHEN** 构造 `ShaderVariant`，含 `{"USE_SHADOWS", 1}` 与 `{"NUM_LIGHTS", 4}`
- **THEN** 两者是同一类型条目，无强/弱区分

### Requirement: bitmask key 与数据驱动 schema

`ShaderVariantKey` SHALL 为 128bit bitmask（`uint64_t words[2]` + `totalBits`），`totalBits > 128` 时 schema 构建 SHALL 报错。`ShaderVariantSchema` SHALL 数据驱动：source 为 `Name` tag（非 enum），位布局/宽度由 schema 定义。

#### Scenario: 越界报错

- **WHEN** schema 的 `totalBits > 128`
- **THEN** 构建失败（报错），不静默截断

#### Scenario: 多来源 OR 组合

- **WHEN** 不同 source 拥有不相交位区间，各自 `Set` 后 `operator|=`
- **THEN** 组合结果等于无冲突拼接

### Requirement: 全局 pipeline 位预留

`GlobalVariantLayout` SHALL 定义引擎级 pipeline 位预留区（固定 bit 位置），所有 shader schema 的 per-shader 位 SHALL 排在预留区之后；PipelinePass SHALL 只用 `GlobalVariantLayout` 设 pipeline 位，不依赖具体 shader schema。

#### Scenario: pipeline 位跨 shader 一致

- **WHEN** 两个不同 shader 的 schema 都引用同一 `GlobalVariantLayout`
- **THEN** `"shadows"` 在两个 shader 里映射到相同 bit 位

### Requirement: 机制由 shader 声明决定

编译层 SHALL 对每个 key 反射解析：命中 `[SpecializationConstant]` 同名声明 → spec 常量；否则 → `-D name=value` 宏。SPIRV/MSL spec 常量 SHALL 走 native specialize（同二进制），DXIL SHALL 折叠。

#### Scenario: spec vs 宏解析

- **WHEN** key `"NUM_LIGHTS"` 在 shader 里声明为 `[SpecializationConstant(0)]`，key `"USE_SHADOWS"` 未声明
- **THEN** 前者走 spec 常量（SPIRV/MSL native、DXIL 折叠），后者走 `-D USE_SHADOWS=...`

### Requirement: 反射是编译产物

反射 SHALL 随编译结果（`{二进制, 反射}`）一起缓存；强变体（宏）SHALL 可改 layout，弱变体（spec 常量）SHALL 不改。反射不构成 cache key 维度。

#### Scenario: 强变体改 layout

- **WHEN** 宏 `#if` 增删一个资源绑定
- **THEN** 不同宏值编译出的反射 layout 可不同（作为编译产物的一部分）

### Requirement: 统一 cache key 与 ShaderCache 接口

`ShaderCacheKey{sourceHash, variantHash, target}` SHALL 统一（variantHash 不分强/弱）。aurora.shader SHALL 预留 `ShaderCache` 接口（Load/Store），`ShaderCompileDesc` 预留 `cache` 字段；本 change 不实现落地。

#### Scenario: 统一 key

- **WHEN** 同一 source + 同一 variant + 同一 target 编译两次
- **THEN** 产生相同 `ShaderCacheKey`；`cache == nullptr` 时走直接编译

