# sky-develop Specification

## Purpose
TBD - created by archiving change sky-develop. Update Purpose after archive.
## Requirements
### Requirement: SKY_DEVELOP 宏表达 develop 构建意图

`SKY_DEVELOP` SHALL 为一个 0/1 宏，默认 `0`；`SKY_DEVELOP == 1` SHALL 表示 develop/开发构建。未定义时 SHALL 经 `#ifndef` 兜底为 `0`。代码 SHALL 用 `#if SKY_DEVELOP` 判读。

#### Scenario: 默认关闭

- **WHEN** 未显式定义 `SKY_DEVELOP`
- **THEN** `SKY_DEVELOP == 0`

#### Scenario: develop 构建开启

- **WHEN** 构建系统开启 develop 模式（editor/tool 构建）
- **THEN** `SKY_DEVELOP == 1`

### Requirement: SKY_DEVELOP 与 _DEBUG 解耦

`SKY_DEVELOP`（构建意图）SHALL 与 `_DEBUG`（编译器 Debug 配置）正交：Debug 配置 SHALL NOT 隐含 `SKY_DEVELOP == 1`，`SKY_DEVELOP == 1` SHALL NOT 要求 Debug 配置。

#### Scenario: Release 编辑器可 develop

- **WHEN** Release 配置 + editor 构建
- **THEN** `SKY_DEVELOP == 1` 且 `_DEBUG` 未定义

### Requirement: 开发者设施统一挂 SKY_DEVELOP

开发者设施（资源名、额外校验、调试 UI 等）SHALL 以 `SKY_DEVELOP` 为开关；`SKY_ENABLE_RESOURCE_NAME` SHALL 改判 `SKY_DEVELOP`（替换临时的 `_DEBUG`）。

#### Scenario: 资源名跟随 SKY_DEVELOP

- **WHEN** `SKY_DEVELOP == 1`
- **THEN** `SKY_ENABLE_RESOURCE_NAME == 1`（资源名生效）；`SKY_DEVELOP == 0` 时资源名关闭

