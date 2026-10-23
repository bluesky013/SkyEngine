## MODIFIED Requirements

### Requirement: 开发者设施统一挂 SKY_DEVELOP

开发者设施（资源名、调试 UI 等）SHALL 默认以 `SKY_DEVELOP` 为开关；`SKY_ENABLE_RESOURCE_NAME` 的 AUTO 策略 SHALL 判读 `SKY_DEVELOP`。资源命名 SHALL 允许显式 ON/OFF 覆盖；引擎断言与 GPU validation SHALL 使用独立配置策略，不能通过伪造 `_DEBUG` 改变 CRT。Develop 编译配置 SHALL 与 SKY_DEVELOP 设施意图保持区分。

#### Scenario: 资源名跟随 SKY_DEVELOP
- **WHEN** 资源命名策略为 AUTO 且 `SKY_DEVELOP == 1`
- **THEN** `SKY_ENABLE_RESOURCE_NAME == 1`；`SKY_DEVELOP == 0` 时默认关闭

#### Scenario: 显式关闭资源命名
- **WHEN** 编辑器使用 SKY_RESOURCE_NAMES=OFF
- **THEN** 资源命名关闭而其它开发设施和 CRT 配置不被改变
