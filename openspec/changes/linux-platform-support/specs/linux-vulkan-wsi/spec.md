## ADDED Requirements

### Requirement: Vulkan 实例在 Linux 启用 xcb surface 扩展

`VulkanInstance` 在 `SKY_PLATFORM_LINUX` 下创建实例时 SHALL 启用 `VK_KHR_surface` 与 `VK_KHR_xcb_surface` 扩展。扩展列表在非 Linux 平台不得新增任何项。

`engine/aurora/rhi/vulkan/CMakeLists.txt` 在 Linux 下 SHALL 定义 `VK_USE_PLATFORM_XCB_KHR` 并链接 `xcb` 与 `X11-xcb` 库。`VulkanFunctions` 函数指针表 SHALL 包含 `vkCreateXcbSurfaceKHR` 的声明与加载（缺失时返回明确的加载失败错误）。

#### Scenario: Linux 实例创建并携带扩展
- **WHEN** 在 Linux 创建 `Instance`
- **THEN** 实例启用扩展列表包含 `VK_KHR_xcb_surface`；`vkCreateXcbSurfaceKHR` 函数指针非空

#### Scenario: 非 Linux 平台零变化
- **WHEN** 在 Win32 / macOS 创建 `Instance`
- **THEN** 扩展列表与函数指针表与本 change 之前完全一致

### Requirement: SwapChain 从原生 X11 窗口桥接创建 xcb surface

`SwapChain::Descriptor.window` 在 Linux 下的句柄契约定为指向 `XcbWindowHandle` 结构体（定义于 core 共享头 `engine/core/include/core/platform/NativeWindowHandle.h`，`{ void *connection; void *window; }`）：framework 的原生 `LinuxWindow` 创建后以 Xlib `Display*` + `XGetXCBConnection` 取得 `xcb_connection_t*` 并组装持有该结构体；aurora 的 `VulkanSwapChain::CreateSurface` Linux 分支 SHALL 将 `void* window` cast 为 `XcbWindowHandle*` 并调用 `vkCreateXcbSurfaceKHR`。aurora 源码 MUST NOT include 任何窗口系统头（Xlib/SDL）。

当前实现只桥接到句柄（`connection` 可能为空若运行环境缺 `libX11-xcb.so.1`）；surface/swapchain 的 xcb 分支与端到端呈现按任务 8 暂缓。

#### Scenario: 句柄可作为 SwapChain.window 传入
- **WHEN** X11 会话中从 `LinuxWindow` 得到 `GetNativeHandle()` 并作为 `SwapChain::Descriptor.window`
- **THEN** `XcbWindowHandle.connection`/`window` 与窗口/连接一致；aurora 侧无需依赖窗口库

#### Scenario: Wayland 会话下明确报错
- **WHEN** 在非 X11 的显示环境中创建窗口/WSI
- **THEN** 报错信息明确（当前仅支持 X11/xcb），进程不崩溃

### Requirement: 无头环境可验证呈现路径

测试目标在缺少 `DISPLAY` 时 SHALL skip surface/swapchain 用例（不记失败）；在 `DISPLAY` 存在但无物理 GPU 时 MAY 通过 Mesa lavapipe（软件 Vulkan）运行呈现用例。

#### Scenario: 无显示环境跳过呈现用例
- **WHEN** 在无 `DISPLAY` 的环境运行含 SwapChain 用例的测试目标
- **THEN** 相关用例以 skipped 状态结束；headless（不建 swapchain）用例照常运行且结果计入统计

#### Scenario: lavapipe 下呈现用例可运行
- **WHEN** 在 xvfb + Mesa lavapipe 环境运行 SwapChain 用例
- **THEN** Acquire/Present 用例实际执行并通过（不依赖物理 GPU）
