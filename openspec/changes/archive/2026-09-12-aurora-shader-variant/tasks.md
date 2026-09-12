## 1. 变体模型与 key

- [x] 1.1 新增 `engine/aurora/shader/include/aurora/shader/ShaderVariant.h`：`ShaderVariantEntry` / `ShaderVariant` / `ShaderVariantKey` / `ShaderVariantSchema` / `GlobalVariantLayout` / `ShaderCacheKey` / `ShaderCache`
- [x] 1.2 实现 `ShaderVariantKey`：128bit（`uint64_t words[2]`）+ `Set`/`Get`/`operator|=` + `totalBits > 128` 校验
- [x] 1.3 实现 `ShaderVariant::ContentHash`（统一 hash，order-insensitive）+ `ShaderCacheKey` 组合（sourceHash + variantHash + target）
- [x] 1.4 单元测试：越界报错、OR 组合、等价变体 hash 一致

## 2. 数据驱动 schema 与全局位预留

- [x] 2.1 `ShaderVariantSchema`（source = `Name` tag，位布局数据驱动）+ `GlobalVariantLayout`（pipeline 位预留区）
- [x] 2.2 schema 构建校验：per-shader 位排在预留区后、不冲突、`totalBits ≤ 128`
- [x] 2.3 测试：跨 shader pipeline 位一致、冲突/越界报错

## 3. 编译器接入变体

- [x] 3.1 `ShaderCompileDesc` 增加 `const ShaderVariant *variant` 与 `ShaderCache *cache`（并增加 `const ShaderVariantSchema *schema`）
- [x] 3.2 **宏路径**：`ShaderCompilerSlang::Compile` 把宏变体注入 `SessionDesc::preprocessorMacros`（`-D`）；**spec 路径**：`schema.isSpec` 且 SPIRV/MSL → 不注入（留给后端 specialize），DXIL → `-D` 折叠
- [x] 3.3 schema 显式声明 spec 常量 `specId`（`Entry.isSpec`/`specId`），编译层不反射（slang 公开 API 无 `[SpecializationConstant]` 反射）

## 4. 离线 codegen 生成 schema

- [x] 4.1 `ShaderVariantGen`（`Parse` 解析 `@variant` 注释块 + `GenerateHeader` 生成 `.variant.h`）；`ShaderHeaderTool` 增加 `--variant` 模式；自动位布局（从 `kReservedBits` 起）+ 校验（≤128、重名）
- [x] 4.2 pipeline 变体数据驱动：`@reserved N` 指令 + 相对偏移（`Parse` 的 `reservedBits` 改为输出）；`ShaderVariantKey::operator<<=`；`GlobalVariantLayout` 复用 `ShaderVariantSchema` 载体 + `Init` 运行时校验；pipeline 模块用 `ShaderFileSystem` 读 `assets/shaders/slang/config/pipeline_variants.slang`；`ShaderFileSystem` 增加公开 `ReadFile`

## 5. 变体编译测试

- [x] 5.1 强变体宏：同源不同宏值 → 不同二进制（`#if` 裁剪生效）
- [x] 5.2 弱变体：SPIRV spec 值不折叠（同二进制）；DXIL 折叠（编译器注入 `AURORA_TARGET_*` 目标宏 + shader `#if AURORA_TARGET_DXIL` 用 `static const` 折叠）
- [x] 5.3 全局 pipeline 位：`SetPipelineBit` 设位 + OR 组合；`operator<<=` 左移
- [x] 5.4 全量 build + ctest 全绿（AuroraShaderTest 38、AuroraPipelineTest 9 通过）

## 6. 收尾

- [x] 6.1 变更以新增为主，符合仓库风格
- [ ] 6.2 待用户确认后 `openspec archive aurora-shader-variant` 归档本 change
