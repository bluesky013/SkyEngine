## ADDED Requirements

### Requirement: 完整 Pass 资源声明

具体 Pass SHALL 从同一共享头展开 uniform 类型、类型化槽位与期望 schema，保留 set/binding、类型、维度和 sampler 形状。资源-only 声明 SHALL 可用；不支持的声明 SHALL 显式失败，不得静默遗漏。C++ 不再依赖反射生成镜像，构建与类型子集由 refactor-aurora-shared-pass-headers 规定。

#### Scenario: 常量与纹理同源
- **WHEN** Pass 声明常量、纹理和 sampler
- **THEN** C++ 与 shader 消费同一源，期望 schema 与实际目标反射一致

#### Scenario: 仅有资源
- **WHEN** Pass 不含常量块
- **THEN** 仍暴露资源表与类型化槽位，不强制占位常量
