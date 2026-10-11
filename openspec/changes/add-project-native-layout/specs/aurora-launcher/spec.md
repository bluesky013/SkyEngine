# Delta: aurora-launcher

## MODIFIED Requirements

### Requirement: launcher 只加载 AuroraRender（移除 legacy render）

`GameApplication` SHALL 只注册并加载 `AuroraRender` 作为渲染模块，SHALL NOT 再加载 legacy `SkyRender`；the runtime
host SHALL NOT link legacy `RenderAdaptor`. 启动参数 SHALL NOT 需要渲染模块选择开关。

#### Scenario: 启动即加载 aurora

- **WHEN** the runtime host starts（无渲染选择参数）
- **THEN** `ModuleManager` 加载 `AuroraRender`，不加载 `SkyRender`

## REMOVED Requirements

### Requirement: Launcher 编辑器模式

**Reason**: the engine no longer ships a runnable launcher, so the `--app editor` mode is gone; the editor host is
the dedicated `SandboxEditor` executable.

**Migration**: run `SandboxEditor` (`engine/sandbox/app`) for the editor host; there is no `Launcher --app editor`.
