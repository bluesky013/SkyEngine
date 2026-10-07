# Tasks: deterministic-cross-platform-hardening

## 1. 已修（同主题）

- [x] 1.1 MD5 `UINT4` 固定为 32 位（LP64 修复）
- [x] 1.2 Fnv1a 逐字节哈希按 `uint8_t` 处理（char 符号性）

## 2. 构建开关

- [x] 2.1 `cmake/options.cmake` 增加 `SKY_DETERMINISTIC_FP`（默认 OFF）
- [x] 2.2 `cmake/configuration.cmake` 开启时关闭 FP 收缩（`-ffp-contract=off` / `/fp:precise`）并定义 `SKY_DETERMINISTIC_FP=1`
- [x] 2.3 默认（OFF）不改变现有构建与数值结果

## 3. 审计（结论记录）

- [x] 3.1 确认未启用 fast-math
- [x] 3.2 确认 `SKY_MATH_SIMD` 默认 OFF
- [x] 3.3 确认复制快照按序输出（sort + vector 序）
- [x] 3.4 `Memory.h` 的 `unsigned long` 为 MSVC 内建规定类型（非宽度敏感）
- [ ] 3.5（后续）确定性后端落地时：固定归约顺序 + 跨平台 trace 测试（见 `physics-determinism` reserved 项）
