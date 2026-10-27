## Context

当前基线（rebase 到 `dev_refactor_rhi@60079ac7`）：

- **平台层已原生化**：上游移除了 SDL，`framework` 按平台选择原生后端（`platform/windows/*`、`platform/macos/*`、`platform/ios/*`、`platform/android/*`），并提供共享 `platform/posix/*`。**无 Linux 分支**。
- **legacy render/shader 已退休**：`engine/render`、`engine/shader` 均已删除；aurora 是唯一渲染栈。旧 `SKY_TYPE_TAG`/`ShaderCompilerTypes` 等文件已不存在。
- **launcher 已项目化**：引擎不再定义 `Launcher`；由 `engine/templates/game/native` + `engine/templates/launcher/<platform>` 生成，`python/package_project.py` 负责 scaffold/generate/package。`CMakePresets.json` 已有 `linux` preset。
- **core 阻塞**：`engine/core/src/util/DynamicModule.cpp` 的 `DYN_PREFIX/DYN_SUFFIX` 仍只覆盖 `_WIN32/__APPLE__/__ANDROID__`，Linux 下未声明直接编译失败；`core/platform/unix/async/Thread.cpp` 的 `SetCurrentThreadName` 仍是空 stub。`Semaphore` 上游已改用 C++20 `std::counting_semaphore`，Linux 可直接用。
- **framework 阻塞**：无 Linux 平台类、无 `NativeWindow::Create` 定义、CMake 无 Linux 分支；`Platform::Init` 链接失败。
- **句柄契约**：`SwapChain::Descriptor.window` 是单个 `void*`；Win32/macOS/Android 自洽单指针，xcb 需要 connection + window 两个句柄。
- **三方**：SDL 已移除；slang 对所有平台开放但 `SLANG_ENABLE_DXIL=ON`。基线 `ba105c59` 把 slang 升到 `v2026.19` 并改为静态链接（`SLANG_LIB_TYPE=STATIC`，新增 `external/fast_float` 子模块）；基线 `eead6b6e` 用 **Jolt 替换 Bullet**（移除 bullet3 包与插件）。静态库（jolt/recast/enet）被链接进共享插件，Linux 需 `-fPIC`；Jolt 在 Linux 还需 `CPP_RTTI_ENABLED=ON`（插件子类需要基类 typeinfo）；Linux 大小写敏感暴露 `FindSlang`/`Findsfmt` 的名字问题。
- 上游 `linux-platform-backend` spec 只要求 headless worker（Core/Framework/AssetTool），窗口化 Linux 明确 out of scope；本 change 在其上补齐窗口/WSI 前能力。

## Goals / Non-Goals

**Goals:**
- Linux 下 `cmake configure + build` 通过（engine 目标与 project-owned Launcher）
- 原生 X11 窗口 + 事件循环可运行；`Platform::Init`/`NativeWindow::Create` 有 Linux 定义
- aurora Vulkan 能加载并初始化 instance/device；xcb surface 代码路径设计就绪
- `third_party.py -p Linux -o build_3rd` 产出引擎所需包
- Win32/macOS/iOS/Android 行为不变

**Non-Goals:**
- WSI 端到端 clear-screen / AuroraTest surface 用例（待接显示器统一处理）
- Wayland
- 编辑器/Sandbox Linux 全链路
- 真实 Linux cook（依赖 builder 模块后续适配）

## Decisions

### 决策 1：framework 采用**原生 X11 后端**（不使用 SDL）

新增 `engine/framework/platform/linux/`：

- `LinuxPlatform : PlatformBase`：`XOpenDisplay`（无显示时仍允许 headless 初始化）、`PollEvent` 用 `XNextEvent` 分发、`steady_clock` 计时、`/proc/self/exe` 路径、`getenv`、`popen`、进程内剪贴板。
- `LinuxWindow : NativeWindow`：`XCreateSimpleWindow`、事件映射（Configure/Focus/Key/Button/Motion/ClientMessage）、鼠标/键盘/文本、光标、指针捕获、DPI（默认 1.0）、多窗口；首个窗口为 main window，关闭主窗口请求退出。
- 事件分发：`LinuxPlatform::PollEvent` 按 XID 从 `NativeWindowManager` 找到 `LinuxWindow`，再 `DispatchNativeEvent`；XID 直接作为引擎 `WindowID`。

**Why:** 上游已把平台层定为原生后端并删除 SDL；复用同一 seam（`Platform`/`NativeWindow`）可让 launcher/editor/runtime 零改动。

### 决策 2：`XcbWindowHandle` 定义在 **core 共享头**

`engine/core/include/core/platform/NativeWindowHandle.h`：`struct XcbWindowHandle { void *connection; void *window; }`。core 是 framework 与 aurora 的共同底层，避免 framework→aurora 依赖倒置。

`LinuxWindow` 用 Xlib `Display*` + `XGetXCBConnection` 得到 `xcb_connection_t*`。宿主机缺 `X11/Xlib-xcb.h` 与 `libX11-xcb.so` dev 链接，故 `XGetXCBConnection` 以运行期 `dlopen("libX11-xcb.so.1")` 解析（仅需 `X11/Xlib.h`、`xcb/xcb.h`）。`winHandle` 指向由窗口持有的结构体。

### 决策 3：Launcher 走 project-owned 模板

引擎不再有 `engine/launcher`。新增：

- `engine/templates/launcher/linux/{main.cpp,CMakeLists.txt}`（`sky_add_exe(TARGET Launcher)` + `OUTPUT_NAME {{PROJECT_NAME}}`）
- `engine/templates/game/native/CMakeLists.txt` 增加 `elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux") add_subdirectory(linux)`
- `python/package_project.py` 的 `LAUNCHER_PLATFORMS` 加 `linux`

`CMakePresets.json` 已有 `linux`（Ninja/Release）preset，无需新增。

### 决策 4：xcb WSI 分阶段，**本阶段暂缓**

已完成：句柄契约、原生窗口、`VK_USE_PLATFORM_*` 之外的全部平台/构建工作。运行日志为 `no platform surface support compiled in`（`VulkanSwapChain::CreateSurface` 落 `#else`）。

后续（接显示器时）：`VulkanInstance` 加 `VK_KHR_surface`+`VK_KHR_xcb_surface`；`VulkanFunctions` 加 `vkCreateXcbSurfaceKHR`；`CreateSurface` xcb 分支；vulkan CMake 定义 `VK_USE_PLATFORM_XCB_KHR` 并链接 `xcb`/`X11-xcb`。

### 决策 5：Linux 构建配置

`cmake/configuration.cmake` Linux 分支：

- `PLATFORM_EXT_LIBS`：`${CMAKE_DL_LIBS}`、`Threads::Threads`
- `CMAKE_POSITION_INDEPENDENT_CODE=ON`（静态库被链接进共享模块）
- `CMAKE_BUILD_RPATH/INSTALL_RPATH="$ORIGIN"`（`DynamicModule` 以裸名 dlopen 同目录模块）

`engine/core/CMakeLists.txt` Linux：`platform/unix/*` + `${CMAKE_DL_LIBS}`。
`engine/framework/CMakeLists.txt` Linux：`platform/linux/*` + `platform/posix/*`，链接 `X11`、`${CMAKE_DL_LIBS}`。

### 决策 6：三方引导的 Linux 修正

- `python/third_party.py`：新增 `options_<platform>` 合并（无该字段行为不变）
- slang：`options_Linux: SLANG_ENABLE_DXIL=OFF`（Linux 走 SPIR-V，不触发 dxc）
- jolt：`options_Linux` PIC + `CPP_RTTI_ENABLED=ON`（插件 `PairFilter : JPH::GroupFilter` 需要基类 typeinfo；否则模块 dlopen 报 undefined symbol）
- recast/enet：`options_Linux` PIC
- `FindSlang.cmake → Findslang.cmake`（`find_package(slang)` 大小写）
- `Findsfmt.cmake` 链接名 `SFMT`（产物 `libSFMT.a`）

### 决策 7：可移植性最小修复

- `engine/framework/include/framework/application/ModuleManager.h` 补 `<memory>`
- `plugins/recast/src/RecastTileGenerator.cpp` 补 `<cstring>`
- `engine/framework/src/world/WorldStreaming.cpp`：泛型 lambda 中的依赖模板调用补 `template`（GCC 要求 `actor->template GetComponent<T>()`）
- `engine/core/include/core/template/Flags.h`、`core/ecs/TypeId.h` 的 Linux/GCC 修复（已在 rebase 前落地并保留）

## Risks / Trade-offs

- **原生 X11 事件映射的完整性**：首版覆盖窗口/焦点/键鼠/滚轮/关闭；IME、拖放、系统级剪贴板留待后续。缓解：以 `NativeWindow`/`PlatformBase` 既有默认行为兜底（不支持的回调为 no-op/失败）。
- **无 `X11-xcb` dev 包**：以 `dlopen` 规避；若运行环境缺 `libX11-xcb.so.1`，WSI 句柄为空（窗口仍可创建）。
- **上游仍把窗口化 Linux 视为 out of scope**：本 change 的窗口层与上游 headless spec 并存，不改变其 worker 路径。
- **cpython 三方在 Linux 构建失败**：默认 `SKY_BUILD_PYTHON=OFF`，不阻塞；启用 python 插件前需单独修复。
- **WSI 暂缓**：当前无法呈现；接显示器后按决策 4 补齐。

## Migration Plan

纯增量，无调用方迁移。顺序：

1. `third_party.py -p Linux -o build_3rd`（slang/PIC/大小写修复）
2. core Linux（DynamicModule/Thread/CMake）
3. framework 原生 X11（Platform/Window/CMake）
4. project-owned Linux launcher 模板 + packaging CLI
5. configure/build engine 与示例工程；运行验证窗口+事件循环
6. 接显示器后补 xcb WSI + surface 测试

## Open Questions

- **Wayland**：后续按 `wl_display*`/`wl_surface*` 增加 `VK_KHR_wayland_surface` 分支。
- **系统剪贴板/IME**：X11 selection/IME 需要在原生窗口层补齐。
- **编辑器 Linux**：涉及 Qt/Sandbox 依赖，单独 change。
