# Delta: physics-determinism

## ADDED Requirements

### Requirement: 确定性浮点构建选项

构建系统 SHALL 提供一个确定性浮点选项（`SKY_DETERMINISTIC_FP`，默认关闭），开启时禁止浮点表达式收缩为 FMA（GNU/Clang `-ffp-contract=off`、MSVC `/fp:precise`），并定义 `SKY_DETERMINISTIC_FP=1`。确定性后端（`Exact` 模式）SHALL 在该选项开启的构建中运行，以避免因架构/编译器的 FMA 收缩产生跨平台差异。

#### Scenario: 默认关闭不改变行为

- **WHEN** 未开启 `SKY_DETERMINISTIC_FP`
- **THEN** 构建与数值结果与既有保持一致

#### Scenario: 开启后禁止 FP 收缩

- **WHEN** 以 `-DSKY_DETERMINISTIC_FP=ON` 配置
- **THEN** C/C++ 编译 SHALL 使用 `-ffp-contract=off`（MSVC：`/fp:precise`）且定义 `SKY_DETERMINISTIC_FP=1`
