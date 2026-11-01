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

### Requirement: 动态库加载在 Linux 使用 lib 前缀与 .so 后缀

`DynamicModule` 在 `SKY_PLATFORM_LINUX` 下 SHALL 使用 `DYN_PREFIX="lib"` 与 `DYN_SUFFIX=".so"` 拼接模块路径，加载/卸载/取符号 SHALL 走 POSIX `dlopen/dlclose/dlsym`（复用既有非 `_WIN32` 分支），链接 `${CMAKE_DL_LIBS}`。

加载失败时 `GetLastError()` SHALL 返回 `dlerror()` 的非空描述（而非空串）。

#### Scenario: Linux 下加载 aurora Vulkan 后端模块
- **WHEN** 在 Linux 运行时使用 `DynamicModule` 加载名称为 `AuroraVulkan` 的模块
- **THEN** 实际打开的文件路径为 `libAuroraVulkan.so`；文件存在时加载成功，不存在时 `GetLastError()` 返回包含具体原因的字符串

#### Scenario: 进程内符号解析
- **WHEN** 对已加载模块调用 `GetSymbol("某个已导出符号")`
- **THEN** 返回非空函数指针；对不存在符号返回 nullptr 且不崩溃

### Requirement: Unix 线程命名与信号量可用

`core/platform/unix/async/Thread.cpp` 的 `SetCurrentThreadName` SHALL 用 `pthread_setname_np` 实现：Linux 签名（带 `pthread_t` 参数，名字截断至 15 字节 + `\0`）与 macOS 签名（仅 name）用 `__APPLE__` 宏区分。

`Semaphore` 上游已改用 C++20 `std::counting_semaphore`（`platform/unix/async/Semaphore.cpp`），Linux 直接可用，本 change 不再单独实现 POSIX 无名信号量。

#### Scenario: 线程命名生效
- **WHEN** 在 Linux 上命名工作线程
- **THEN** 线程名被截断到 15 字节并设置成功，不崩溃

#### Scenario: 信号量计数语义正确
- **WHEN** 初始 count=0 的信号量上 Release 两次、Wait 一次
- **THEN** 第一次 Wait 立即返回；连续两次 Wait 第二次阻塞（直至另有 Release）

#### Scenario: Linux 构建包含 unix 平台源文件
- **WHEN** 在 Linux 配置并构建 core 模块
- **THEN** `platform/unix/async/{Semaphore,Thread}.cpp` 被编译进目标；core 编译链接无未定义符号

### Requirement: core CMake 增加 Linux 平台分支

`engine/core/CMakeLists.txt` SHALL 在 `WIN32/APPLE/ANDROID` 之外增加 Linux 分支：选择 `platform/unix/async/` 源文件加入 `CORE_PLATFORM_SRC`，并链接 `${CMAKE_DL_LIBS}`。Linux 分支不得破坏既有三个平台的源文件选择与链接行为。

#### Scenario: Linux 配置包含平台源与 dl 链接
- **WHEN** 在 Linux 运行 CMake configure 并构建 core
- **THEN** `platform/unix/async/` 源文件出现在 core 的编译单元中；链接命令包含 `libdl`（`${CMAKE_DL_LIBS}`）

#### Scenario: 既有平台配置不变
- **WHEN** 在 Win32 / macOS / Android 运行 CMake configure
- **THEN** core 的源文件列表与链接库与本 change 之前完全一致
