# aurora-launcher Specification

## Purpose
TBD - created by archiving change aurora-launcher-integration. Update Purpose after archive.
## Requirements
### Requirement: AuroraRender 是可被 launcher 加载的动态模块

`engine/aurora/adaptor` SHALL 产出 `AuroraRender` SHARED 目标，SHALL 通过 `REGISTER_MODULE` 导出 `StartModule`/`StopModule` 并实现 `sky::IModule`。模块 SHALL 与 `SkyRender` 并存，SHALL NOT 改变 `SkyRender` 的默认行为。

#### Scenario: 模块被 ModuleManager 发现

- **WHEN** launcher 选择 aurora 渲染模块
- **THEN** `ModuleManager` 能加载 `AuroraRender` 并调用其 `Init`

### Requirement: 模块初始化 aurora 设备

`AuroraModule::Init` SHALL 从启动参数解析 RHI API，创建 `aurora::Instance` 与 `Device`，并创建 `DeviceFrameContext` 与图形 `CommandPool`/`CommandBuffer`。设备创建失败时 SHALL 记录并保持在未初始化状态。

#### Scenario: 指定 RHI API

- **WHEN** 启动参数包含 `-r vulkan` / `-r dx12`
- **THEN** 用对应 API 初始化 `Instance`

#### Scenario: 初始化失败安全

- **WHEN** `Instance`/`Device` 创建失败
- **THEN** `Tick`/`Shutdown` 成为 no-op，不崩溃

### Requirement: 模块用平台窗口创建 viewport

`AuroraModule::Start` SHALL 从平台取原生窗口 handle（`Platform::Get()->GetMainWinHandle()`），SHALL 以该 handle 初始化 `ClientViewport`（内部创建 SwapChain 与 per-image 二元信号量）。

#### Scenario: 创建 swapchain viewport

- **WHEN** 模块 `Start`
- **THEN** 存在一个 `ClientViewport`，其 SwapChain 使用平台主窗口 handle

### Requirement: Tick 驱动最小出帧闭环

`AuroraModule::Tick` SHALL 每帧执行：frame begin → viewport `Begin`/`Acquire` → backbuffer `UNDEFINED → COLOR_ATTACHMENT` barrier → `BeginRendering`（`LoadOp::CLEAR`）→ `COLOR_ATTACHMENT → PRESENT` barrier → `Submit`（wait acquire sema、signal render-done sema、frame fence）→ `Release`/present → frame end。SHALL 无 window 或 `Begin`/`Acquire` 失败时取消该帧（不 present）。

#### Scenario: 出帧并呈现

- **WHEN** viewport 可用且 Acquire 成功
- **THEN** backbuffer 被 clear、提交并以 render-done 信号量 present

#### Scenario: acquire 失败取消

- **WHEN** `Begin()` 或 `Acquire()` 返回 false（最小化 / OUT_OF_DATE / LOST）
- **THEN** 不提交、不 present，且不崩溃

### Requirement: launcher 只加载 AuroraRender（移除 legacy render）

`GameApplication` SHALL 只注册并加载 `AuroraRender` 作为渲染模块，SHALL NOT 再加载 legacy `SkyRender`；`Launcher` SHALL NOT 链接 legacy `RenderAdaptor`。启动参数 SHALL NOT 需要渲染模块选择开关。

#### Scenario: 启动即加载 aurora

- **WHEN** launcher 启动（无渲染选择参数）
- **THEN** `ModuleManager` 加载 `AuroraRender`，不加载 `SkyRender`

### Requirement: 窗口 handle 由宿主提供

宿主应用 SHALL 通过 `ISystemNotify::GetMainWindowHandle()` 暴露主窗口原生 handle；渲染模块 SHALL 优先从该接口取 handle，SHALL NOT 只依赖 `Platform::GetMainWinHandle()`（Windows SDL 后端返回 nullptr）。

#### Scenario: 用宿主窗口建 swapchain

- **WHEN** 模块 `Start`
- **THEN** 从 `ISystemNotify` 取到主窗口 handle 并初始化 `ClientViewport`

### Requirement: 引擎内置 configs

引擎 SHALL 提供内置 `configs/`（仓库根，`modules_game.json` / `modules_editor.json` / `asset_build_presets.json` / `render_preload_assets.json`），并在构建时部署到可执行文件旁的 `configs/`。`GameApplication` SHALL 在项目 workFs 找不到 `configs/modules_game.json` 时回退到内置配置，使 launcher 无需外部工程即可启动。

#### Scenario: 无工程启动

- **WHEN** 直接运行 launcher 且无 `-p` 工程（或工程无 configs）
- **THEN** 从可执行文件旁的内置 `configs/modules_game.json` 加载模块配置

### Requirement: Launcher 编辑器模式

The launcher SHALL support an editor application mode selected by a start argument (`--app editor`) that runs the
editor host, in addition to its game and XR modes.

#### Scenario: Start the editor
- **WHEN** the launcher is started with `--app editor`
- **THEN** it SHALL run the editor host and open the editor window

#### Scenario: Default remains the game mode
- **WHEN** the launcher is started without `--app editor`
- **THEN** it SHALL run the game (or XR) application as before

### Requirement: Launcher 游戏模式从部署的 configs 启动

Outside editor/tool/Android builds the game application SHALL use the executable bundle path as its work file
system, so the builtin `configs/` deployed next to the launcher are found and the launcher boots without a
project directory.

#### Scenario: Boot from deployed configs
- **WHEN** the launcher runs in game mode with `configs/modules_game.json` next to the executable
- **THEN** it SHALL load the config from the bundle path and register its modules without crashing

