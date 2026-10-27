## ADDED Requirements

### Requirement: 原生 X11 平台初始化

`engine/framework/platform/linux/LinuxPlatform.h/.cpp` SHALL 提供 `LinuxPlatform`，继承 `PlatformBase`（不使用 SDL），实现 `Platform::Init`：`XOpenDisplay`、事件循环、计时、可执行文件路径、环境变量、`RunCmd` 与剪贴板；`GetType()` SHALL 返回 `PlatformType::Linux`。

无显示环境（headless worker）下 `Platform::Init` SHALL 仍返回成功（窗口创建随后明确失败），不得因缺少 X 显示而崩溃或链接失败。

#### Scenario: 初始化并查询平台类型
- **WHEN** Linux 下调用 `Platform::Get()->Init()`
- **THEN** 返回成功；`GetType()` 为 `Linux`；internal/bundle 路径可从可执行文件位置解析

#### Scenario: 无显示环境 headless 初始化
- **WHEN** 在无 `DISPLAY` 的环境调用 `Platform::Init()`
- **THEN** 返回成功（仅告警）；`PollEvent` 为空操作；不崩溃

#### Scenario: 事件轮询与计时可用
- **WHEN** Init 后进入帧循环调用 `PollEvent` 与高精度时钟查询
- **THEN** 窗口事件能送达；时钟单调递增

### Requirement: 原生 X11 窗口与句柄

`engine/framework/platform/linux/LinuxWindow.h/.cpp` SHALL 提供 `LinuxWindow : NativeWindow`，`NativeWindow::Create` 在 Linux 返回它。窗口创建使用 Xlib；`GetNativeHandle()` 返回指向 `XcbWindowHandle`（定义于 core 共享头 `core/platform/NativeWindowHandle.h`，由窗口对象持有生命周期）的指针。

`XcbWindowHandle.connection` SHALL 经 `XGetXCBConnection` 获得；为不依赖 `X11-xcb` dev 包，可用运行期 `dlopen("libX11-xcb.so.1")` 解析。

#### Scenario: 创建窗口并取句柄
- **WHEN** 以 1280×720 标题 "SkyEngine" 调用 `NativeWindow::Create`
- **THEN** 返回非空窗口；`GetWidth/GetHeight` 返回 1280/720；`GetNativeHandle()` 返回非空 `XcbWindowHandle*`，可直接传给 aurora `SwapChain::Descriptor.window`

#### Scenario: 窗口事件映射
- **WHEN** 窗口被 resize/聚焦/按键/鼠标操作
- **THEN** `IWindowEvent`/`IKeyboardEvent`/`IMouseEvent` 广播对应事件；主窗口关闭请求退出事件循环

#### Scenario: 多窗口
- **WHEN** 创建第二个 `NativeWindow`
- **THEN** 两个窗口各自收到正确的事件；关闭非主窗口不影响应用继续运行

### Requirement: project-owned Linux Launcher

引擎 SHALL NOT 定义 `Launcher` 目标。`engine/templates/launcher/linux/` SHALL 提供 `main.cpp` 与 `CMakeLists.txt`（`sky_add_exe(TARGET Launcher)`，产物名为 `{{PROJECT_NAME}}`）；`engine/templates/game/native/CMakeLists.txt` SHALL 在 Linux 分支 `add_subdirectory(linux)`；`python/package_project.py` 的 `LAUNCHER_PLATFORMS` SHALL 含 `linux`。

#### Scenario: 生成并构建 Linux 工程
- **WHEN** `package_project.py create` 生成工程后，在 `native/` 用 `linux` preset configure/build
- **THEN** `Launcher` 目标构建成功，产物为 `{{PROJECT_NAME}}`，能创建窗口进入事件循环

#### Scenario: 引擎单独构建不定义 Launcher
- **WHEN** 仅配置引擎根工程
- **THEN** 不要求存在 `Launcher` 目标
