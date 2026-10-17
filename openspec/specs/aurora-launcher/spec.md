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

AuroraModule::Init SHALL 解析 RHI API 并通过 IRenderer 初始化 provider；Device、FrameSlots 和 command 资源 SHALL 由渲染端持有。初始化 SHALL 返回明确成功/失败结果，失败时不进入 GPU 帧循环；Shutdown SHALL 安全清理已创建部分。

#### Scenario: 指定 RHI API
- **WHEN** 启动参数包含 -r vulkan 或 -r dx12
- **THEN** Renderer 使用对应 API 初始化设备，AuroraModule 不再自行创建另一套设备

#### Scenario: 初始化失败安全
- **WHEN** provider 或设备初始化失败
- **THEN** 错误反馈给宿主，Tick 不发起 GPU 操作，Shutdown 可安全调用

### Requirement: 模块用平台窗口创建 viewport

AuroraModule::Start SHALL 优先从 ISystemNotify 取得主窗口原生 handle，并通过 Renderer 提交窗口描述创建 Viewport。平台窗口创建、事件和销毁 SHALL 留在宿主线程；原生窗口 SHALL 保活至 Renderer 返回销毁确认。

#### Scenario: 创建 swapchain viewport
- **WHEN** 模块 Start 且宿主提供有效窗口 handle
- **THEN** Renderer 创建关联该窗口的 viewport；宿主不直接持有或操作 swapchain

#### Scenario: 原生关闭请求与应用析构
- **WHEN** Windows 宿主收到 WM_CLOSE 或开始退出应用
- **THEN** 渲染中的原生窗口延迟销毁，宿主在渲染退休期间继续处理平台消息；应用在窗口成员析构前完成模块 Shutdown，不能仅把退出放在基类析构函数

#### Scenario: 选择整套宿主模式
- **WHEN** 已迁移平台启动 launcher，或尚未迁移平台启动原宿主
- **THEN** 在初始化时选择唯一一套帧循环；旧宿主不得借用新 Renderer 的 Device 另建 command/frame 资源

### Requirement: Tick 驱动最小出帧闭环

AuroraModule::Tick SHALL 收集状态/命令并提交 FramePacket，由 Renderer 处理 acquire、RDG、submit 与 present。无场景内容的有效窗口 SHALL 支持清屏；有效场景 SHALL 走实际 pipeline。零尺寸 SHALL 不 acquire，单窗口失败 SHALL 不阻断其它输出或命令；成功 acquire 后的失败 SHALL 安全收尾，不直接 Release 一个会等待未 signal semaphore 的 image。

#### Scenario: 出帧并呈现
- **WHEN** viewport 可用且提交 packet
- **THEN** Renderer 完成真实绘制或空场景清屏并呈现，宿主 Tick 不直接编码 GPU 命令

#### Scenario: packet 背压与尺寸变化
- **WHEN** TrySubmit 返回 BUSY 且窗口继续 resize
- **THEN** 保留未接纳 packet 的身份和数据，接纳后再用后续 packet 提交最新尺寸，不修改排队帧快照

#### Scenario: acquire 失败取消单输出
- **WHEN** 一个 viewport Begin/Acquire 失败
- **THEN** 该输出不 present，其余输出继续工作且 lifecycle 命令仍生效

#### Scenario: 零尺寸不 acquire
- **WHEN** surface extent 为零
- **THEN** Renderer 跳过该输出 acquire，恢复尺寸后可正常继续

#### Scenario: 已获取图像的失败收尾
- **WHEN** acquire 成功后建图或提交失败
- **THEN** Renderer 走有效收尾提交或后端错误退休路径，不产生等待未 signal 对象的 Present

### Requirement: launcher 只加载 AuroraRender（移除 legacy render）

`GameApplication` SHALL 只注册并加载 `AuroraRender` 作为渲染模块，SHALL NOT 再加载 legacy `SkyRender`；the runtime
host SHALL NOT link legacy `RenderAdaptor`. 启动参数 SHALL NOT 需要渲染模块选择开关。

#### Scenario: 启动即加载 aurora

- **WHEN** the runtime host starts（无渲染选择参数）
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

### Requirement: Launcher 游戏模式从部署的 configs 启动

Outside editor/tool/Android builds the game application SHALL use the executable bundle path as its work file
system, so the builtin `configs/` deployed next to the launcher are found and the launcher boots without a
project directory.

#### Scenario: Boot from deployed configs
- **WHEN** the launcher runs in game mode with `configs/modules_game.json` next to the executable
- **THEN** it SHALL load the config from the bundle path and register its modules without crashing

