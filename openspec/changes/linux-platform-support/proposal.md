## Why

上游 `dev_refactor_rhi` 已退休旧 render/shader/Qt 编辑器，平台层改为**原生后端**（Win32 / Cocoa / UIKit / GameActivity，**不再使用 SDL**），launcher 改为 **project-owned**（`engine/templates/launcher/<platform>` + `engine/templates/game/native` + `python/package_project.py`）。但 Linux 仍缺原生窗口后端：`Platform::Init`、`NativeWindow::Create`、平台 CMake 分支均没有 Linux 实现，aurora Vulkan 也缺 X11/xcb surface。

本 change 把 Linux 适配到当前架构：原生 X11 平台/窗口、project-owned Linux launcher、core/framework 构建分支、以及三方引导在 Linux 的可用性。aurora 的 Vulkan xcb WSI 完成到「surface 前」的状态，WSI 端到端留待接显示器时统一处理。

## What Changes

- **core 平台胶水**：`DynamicModule` 增加 Linux `lib/.so` 命名（`GetLastError` 已有 `dlerror` 分支）；unix `Thread::SetCurrentThreadName` 用 `pthread_setname_np` 填实；`engine/core/CMakeLists.txt` 增加 Linux 分支并链接 `${CMAKE_DL_LIBS}`（Unix 信号量上游已改为 `std::counting_semaphore`，无需改）
- **framework 原生平台层（无 SDL）**：新增 `engine/framework/platform/linux/LinuxPlatform`（X11 事件循环/计时/路径/环境变量/RunCmd/剪贴板）与 `LinuxWindow`（X11 窗口、事件映射、光标、DPI、多窗口）；`engine/framework/CMakeLists.txt` 增加 Linux 分支（`platform/linux/*` + 共享 `platform/posix/*`，链接 `X11`/`${CMAKE_DL_LIBS}`）
- **WSI 句柄契约**：core 共享头新增 `XcbWindowHandle`（`{connection, window}`）；`LinuxWindow` 经 `SDL_GetWindowWMInfo` 的等价路径（Xlib `Display*` + `XGetXCBConnection`，后者以 `dlopen` 解析，避免 X11-xcb dev 包）组装并由窗口持有；aurora 只认裸句柄
- **project-owned Linux Launcher**：新增 `engine/templates/launcher/linux/{main.cpp,CMakeLists.txt}`，`engine/templates/game/native/CMakeLists.txt` 增加 Linux 分支，`python/package_project.py` 的 `LAUNCHER_PLATFORMS` 加入 `linux`
- **aurora Vulkan xcb WSI（暂缓）**：`VulkanInstance` 扩展、`VulkanFunctions` 的 `vkCreateXcbSurfaceKHR`、`VulkanSwapChain::CreateSurface` xcb 分支、vulkan CMake 的 `VK_USE_PLATFORM_XCB_KHR`。当前运行止于 `no platform surface support compiled in`
- **构建配置**：`cmake/configuration.cmake` 增加 Linux（`dl`/pthread、`CMAKE_POSITION_INDEPENDENT_CODE`、`$ORIGIN` rpath）
- **三方引导**：`third_party.py` 的 `options_<platform>` 合并；slang Linux `SLANG_ENABLE_DXIL=OFF`；Jolt（上游替换 Bullet）Linux PIC + RTTI；recast/enet Linux PIC；`FindSlang.cmake → Findslang.cmake`、`Findsfmt` 使用 `SFMT`（Linux 大小写敏感）

## Capabilities

### New Capabilities
- `linux-core-platform`：Linux 下 core 的编译/运行动态库与线程原语
- `linux-application-shell`：原生 X11 平台/窗口 + project-owned Linux launcher
- `linux-vulkan-wsi`：aurora Vulkan 在 Linux X11（xcb）上的 surface/swapchain（surface 前实现完成，端到端待显示器）
- `linux-thirdparty-bootstrap`：第三方引导在 Linux 的闭环（slang/PIC/大小写）

### Modified Capabilities
（无）

## Impact

- **引擎源码**：`engine/core/{src/util/DynamicModule.cpp,platform/unix/async/Thread.cpp,include/core/platform/NativeWindowHandle.h}`、`engine/framework/platform/linux/*`（新增）、`engine/framework/{CMakeLists.txt,include/framework/application/ModuleManager.h}`、`engine/aurora/rhi/vulkan/*`（xcb WSI，暂缓）
- **模板/工具**：`engine/templates/launcher/linux/*`（新增）、`engine/templates/game/native/CMakeLists.txt`、`python/package_project.py`、`python/third_party.py`
- **构建脚本**：`cmake/configuration.cmake`、`cmake/thirdparty.json`、`cmake/thirdparty/{Findslang,Findsfmt}.cmake`
- **对既有平台的影响**：纯增量；Win32/macOS/iOS/Android 路径不改
- **明确不做**：Wayland、编辑器 Linux 全链路、真实 Linux cook（依赖 builder 模块后续适配）、WSI 端到端（待接显示器）
