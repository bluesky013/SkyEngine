## MODIFIED Requirements

### Requirement: 统一 cache key 与 ShaderCache 接口

`ShaderCacheKey` SHALL 统一（variantHash 不分强/弱）并扩展为 `{sourceHash, variantHash, target, stage, entryHash, layoutFp, schemaFp, toolchainFp}`。`ShaderCache`（Load/Store）接口 SHALL 由 `shader-cache` change 落地为内容寻址存储后端（offline 只读 / local 可写两 root）；`ShaderCompileDesc` 的 `cache` 字段 SHALL 被使用。

#### Scenario: 统一 key

- **WHEN** 同一 source + 同一 variant + 同一 target + 同一 layout/schema/toolchain 编译两次
- **THEN** 产生相同 `ShaderCacheKey`；`cache == nullptr` 时走直接编译

#### Scenario: key 含工具链与布局

- **WHEN** source / variant / target 相同，但 `toolchainFp` 或 `layoutFp` 不同
- **THEN** `ShaderCacheKey` 不同，不会命中旧产物
