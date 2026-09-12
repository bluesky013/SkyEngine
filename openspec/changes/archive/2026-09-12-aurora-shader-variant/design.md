## Context

现状：

- `ShaderCompilerSlang::Compile(ShaderCompileDesc{source, entry, stage, target, fileSystem})` → `ShaderCompileResult{data, reflection, errorInfo}`，无变体概念。
- slang 支持：`CompilerOptionEntry::MacroDefine`（`-D name=value`，编译期 `#if` 裁剪）；`[SpecializationConstant(id)]` + `getEntryPointCode` specialization（弱变体）。
- 反射 `ShaderReflection`（`set/binding/type/stage`，含 `ShaderBlockLayout`）已在 `aurora/rhi`，是 C++↔shader 布局事实源。
- 已有离线 codegen（`ShaderHeaderTool`，`.slang` → C++ 头）与 `ShaderBlockGen::ContentHash` cache key 思想。
- 三层 RG：Global(set0)/Pass(set1)/Batch(set2)；`ShaderResourceType` 与 SPIR-V 资源类型对齐。

## Goals / Non-Goals

**Goals:**

- 统一变体模型：单一 `ShaderVariant{Name→value}`，接口/cache key 不区分强/弱。
- bitmask key（128bit + 越界校验）+ 数据驱动 schema（source = `Name` tag）。
- 多来源（pipeline/vertex/batch）不相交位 OR 组合成最终 key；pipeline 位全局预留。
- 机制（宏折叠 vs spec constant）由 shader 声明决定，后端处理；DX12 弱变体折叠。
- 预留离线 `ShaderCache` 接口；schema 数据驱动（离线 codegen / 运行时 JSON）。

**Non-Goals:**

- 不实现 `ShaderCache` 落地（只定义接口）。
- 不实现资源绑定（`aurora-resource-group`）；仅确立 heap+offset 抽象方向。
- 不强制「反射 variant-invariant」（强变体可改 layout，见 D9）。

## Decisions

### D1. 统一变体模型（无强/弱类型）

```cpp
struct ShaderVariantEntry {
    Name     key;      // 宏名 / spec 常量名（与 shader 声明一对一）
    uint32_t value;
};
struct ShaderVariant {
    std::vector<ShaderVariantEntry> entries;
};
```

接口层、cache key 层**强/弱完全一致**——都是 name→value。差异只在后端处理（D8）。

### D2. bitmask key（128bit + 校验）

```cpp
struct ShaderVariantKey {
    uint64_t words[2];  // 128-bit 容量
    uint32_t totalBits; // 来自 schema；totalBits > 128 时 schema 构建报错
    void Set(const ShaderVariantSchema &s, Name key, uint32_t value);
    uint32_t Get(const ShaderVariantSchema &s, Name key) const;
    ShaderVariantKey &operator|=(const ShaderVariantKey &o);  // 不相交 → OR 即拼接
};
```

固定 128bit 容量（`uint64_t words[2]`），比较/哈希走整数；超出 128bit 视为变体设计失控，schema 构建期报错。

### D3. 数据驱动 schema（source = `Name` tag）

```cpp
struct ShaderVariantSchema {
    struct Source { Name name; uint16_t bitOffset; uint8_t bitWidth; };
    struct Entry  { Name key; Name source; uint16_t bitOffset; uint8_t bitWidth; uint32_t defaultValue; };
    std::vector<Source> sources;
    std::vector<Entry>  entries;
    uint32_t totalBits;
};
```

- **source 是 `Name` tag**（`"pipeline"`/`"vertex"`/`"batch"`...），不写死 enum，可自定义扩展
- **位宽、分区全由 schema 数据驱动**，不硬编码
- **schema 不手写 C++**：离线 codegen（`.slang` 注释块 `@variant` → 生成 `.variant.h`）或运行时 JSON 反序列化，双通道
- spec 常量 `defaultValue` **反射自动给**（宏默认 0）

### D4. 多来源组合（不相交位 OR）

各 source 拥有不相交 bit 区间，`operator|=` 组合；schema 保证不相交，OR == 无冲突拼接。

### D5. 全局 pipeline 位预留（数据驱动，运行时校验）

pipeline 开关的 bit 位**所有 shader 统一**，但键**不硬编码在引擎**：由独立 `.slang`（`assets/shaders/slang/config/pipeline_variants.slang`）用 `@variant` 语法声明，`@reserved N` 声明预留预算。

```slang
// ===== @variant =====
// @reserved 16
// @source pipeline
//   SHADOWS : bool = 0
// ===================
```

- `GlobalVariantLayout`（aurora/pipeline）复用 `ShaderVariantSchema` 载体，`Init` 运行时校验 `schema.totalBits ≤ reservedBits`
- pipeline 模块 init 用 `ShaderFileSystem` 读文件（与 shader 加载同源）→ `Parse` → 校验，pipeline 维护实例
- per-shader schema 用**相对偏移**（0 起），组合时 per-shader 位左移 `reservedBits`（`ShaderVariantKey::operator<<=`）

### D6. 来源划分

| 来源 | 决定因素 | 例子 |
|---|---|---|
| `pipeline` | `PipelinePass`（场景级） | shadows / ibl |
| `vertex` | **RenderItem 的 mesh 顶点数据 ∩ 用户设置** | skin / vertexColor / uvN / tangent |
| `batch` | 每 draw/材质（batch 层） | emissive / normal |

### D7. 机制由 shader 声明决定（反射解析）

编译层对每个 key：反射查 `[SpecializationConstant]` 同名声明 → 命中走 spec 常量，否则 `-D name=value`（宏）。

```cpp
std::unordered_map<Name, uint32_t> specIds;   // 反射建表：spec 名 → id
for (auto &e : variant.entries) {
    if (specIds.contains(e.key)) applySpecConstant(specIds[e.key], e.value);
    else addMacroDefine(e.key.GetStr(), e.value);
}
```

### D8. 后端处理差异（fold vs native specialize）

| key 类型 | SPIRV | MSL | DXIL |
|---|---|---|---|
| 宏 | 折叠（重编译） | 折叠 | 折叠 |
| spec 常量 | `VkSpecializationInfo`（同二进制） | `MTLFunctionConstantValues`（同二进制） | **折叠**（无 spec 常量，退化强变体） |

- DX12 弱变体 = fold，**不用 root constant**（spec=编译期常量，root const=运行时 uniform，语义不同）
- 弱变体 ≠ uniform：真运行时值走 batch UBO

### D9. 反射 = 编译产物（layout 可变性是 shader 性质）

反射是**编译结果的一部分**（随 `{二进制, 反射}` 一起缓存），不是 cache key 维度：

- 弱变体（spec 常量）天然不改 layout
- 强变体（宏）**可以**改 layout（`#if` 增删资源）

这**不**构成 cache key 的区分；只是每次编译产出的反射内容可能随宏变。资源始终在母体声明（D11），但宏可增删。

### D10. cache 统一 key + `ShaderCache` 接口

```cpp
struct ShaderCacheKey {
    uint64_t     sourceHash;   // 母体源码 hash
    uint64_t     variantHash;  // 变体 name→value 统一 hash（不分强/弱）
    ShaderTarget target;
};

class ShaderCache {
public:
    virtual ~ShaderCache() = default;
    virtual bool Load(const ShaderCacheKey &key, ShaderCompileResult &out) = 0;
    virtual void Store(const ShaderCacheKey &key, const ShaderCompileResult &r) = 0;
};
```

- cache key 统一（`source + target + variant`），强/弱不区分
- spec 值在 SPIRV/MSL 上不改变二进制 → 编译器可内部去重（可选优化，不影响 key 统一）
- `ShaderCompileDesc` 预留 `ShaderCache *cache = nullptr`，本 change 只定义不实现

### D11. shader 侧写法约定

1. **资源永远在母体声明，不进 `#if`**（spec 常量不能删资源；宏可增删，但基础 tier 资源始终在）
2. **强变体**：`#if MACRO`（宏名 = key 名）
3. **弱变体**：`[SpecializationConstant(id)]`
4. **key 名 == shader 里宏名/spec 名**（一对一）

### D12. descriptor 映射 + `VK_EXT_descriptor_heap` 预留

- Vulkan：set/binding 直达（`set`→set index，`binding`→binding index）
- DX12：set→space→root param（descriptor table），binding→register→range；sampler 独立 heap
- Metal：set→argument buffer index
- **趋势**：`VK_EXT_descriptor_heap` 使 Vulkan 收敛到 DX12/Metal 的 heap 模型 → **资源绑定抽象按「heap + offset」设计**，实现先用 `VkDescriptorPool`（1.3）过渡，只留换口

## Risks / Trade-offs

- **128bit 上限**：超出即报错，是设计失控信号；真需要超量时用 spec 常量/运行时 uniform 而非 bitmask。
- **强变体可改 layout**：反射不再 variant-invariant，需接受 per-permutation 反射缓存；若需「反射稳定契约」则靠 review 约束宏不删基础资源。
- **统一 cache key 的冗余**：spec 值在 SPIRV/MSL 上产生同二进制多 key，编译器内部去重缓解。
- **DXIL 弱变体折叠**：丢失「便宜 specialize」收益，DX12 上弱=强；语义仍正确（编译期常量）。
- **DXIL 折叠机制（已落地）**：编译器注入目标宏 `AURORA_TARGET_SPIRV/MSL/DXIL`（0/1），shader 用 `#if AURORA_TARGET_DXIL` 条件声明——DXIL 走 `static const`（`-D` 覆盖折叠），SPIRV/MSL 走 `[SpecializationConstant] const`（spec 常量，值后置）。已验证：SPIRV 同二进制、DXIL 不同二进制。

## Migration Plan

1. 新增 `ShaderVariant.h`：`ShaderVariantEntry`/`ShaderVariant`/`ShaderVariantKey`/`ShaderVariantSchema`/`GlobalVariantLayout`/`ShaderCache`/`ShaderCacheKey`。
2. `ShaderCompileDesc` 增加 `variant` + `cache`；`Compile` 解析 name→宏/spec，注入 `-D` + specialization。
3. `ShaderVariant::ContentHash`（统一 hash）+ `ShaderVariantKey::Set/Get/|=` + 128bit 校验。
4. `ShaderHeaderTool` 扩展：解析 `@variant` 注释块 + `[SpecializationConstant]` 反射 → 生成 `.variant.h`（schema + 位布局 + 全局预留校验）。
5. 测试：强变体宏（不同二进制/反射）+ 弱变体（SPIRV/MSL 同二进制、DXIL 折叠）+ 全局位预留 + 越界报错。
6. 待用户确认后 `openspec archive aurora-shader-variant`。

## Open Questions

- `VK_EXT_descriptor_heap` 落地时机（等生态/驱动覆盖；设计已按 heap+offset 预留）。
- 运行时 JSON 通道的 schema 格式（codegen 首版，JSON 后续）。
