# Change: deterministic-cross-platform-hardening

## Why

跨平台确定性（lockstep / 回滚 / 确定性物理）要求同样的输入在各平台产生**位一致**结果。审计 core/engine 后，发现并修复了「原生类型宽度/符号」这类真实不一致，其余来源做了排查与补齐：

| 项 | 结论 | 处理 |
|---|---|---|
| `unsigned long` 当 32 位字（MD5） | LP64 下是 64 位 → 哈希全错 | **已修**：`uint32_t UINT4` |
| `char` 符号性（Fnv1a 逐字节哈希） | x86 signed / ARM unsigned → 字节 ≥0x80 哈希不同 | **已修**：`static_cast<uint8_t>` |
| fast-math | 未启用 | ✅ 无需处理 |
| `SKY_MATH_SIMD` | 默认 OFF | ✅（若开启需评估归约顺序） |
| FMA 收缩 | clang 默认 `-ffp-contract=on`，随架构/编译器收缩为 fma → 结果可能不同 | **新增开关** `SKY_DETERMINISTIC_FP`（`-ffp-contract=off` / MSVC `/fp:precise`） |
| 复制快照顺序 | `ActorReplicationSource` 对 bucket `std::sort`、成员按 vector 序 | ✅ 已按序输出 |
| 端序 | Murmur3 `memcpy` 读原生序；目标均小端 | ✅ 记录（BE 需显式解码） |
| 指针/地址哈希 | 未用于确定性路径 | ✅ |

## What Changes

- 新增 `SKY_DETERMINISTIC_FP` 构建选项（默认 OFF）：开启时定义 `SKY_DETERMINISTIC_FP=1` 并关闭 FP 收缩（GNU/Clang `-ffp-contract=off`、MSVC `/fp:precise`）。
- 已修复的确定性 bug（MD5 宽度、Fnv1a 符号）随本主题归档说明。

## Capabilities

### Modified Capabilities

- `physics-determinism`: 新增「确定性浮点构建选项」要求——提供关闭 FP 收缩的构建开关，确定性后端 SHALL 在该模式下构建。

## Impact

- `cmake/options.cmake`、`cmake/configuration.cmake`
- `engine/core/src/crypto/md5/MD5.h`、`engine/core/include/core/hash/Fnv1a.h`（已修）
- 默认构建行为不变（选项 OFF）
