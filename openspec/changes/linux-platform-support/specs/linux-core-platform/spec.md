## ADDED Requirements

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
