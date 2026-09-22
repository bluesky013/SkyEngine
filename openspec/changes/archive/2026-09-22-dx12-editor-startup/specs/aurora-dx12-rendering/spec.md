## ADDED Requirements

### Requirement: DX12 调试层 break 策略

DX12 后端在 debug 构建启用 info queue 时，SHALL 仅在附加调试器的情况下对 ERROR / CORRUPTION 级别消息 break；
未附加调试器时 SHALL NOT 终止进程（消息仍被记录，不静默吞掉）。

#### Scenario: 无调试器不致命
- **WHEN** debug 构建下未附加调试器运行，且出现 error 级 DX12 消息
- **THEN** 进程 SHALL 继续运行，而不是在该消息处 break 或退出

#### Scenario: 有调试器保留 break
- **WHEN** 已附加调试器且出现 error 级 DX12 消息
- **THEN** 后端 SHALL 在该消息处 break，便于定位
