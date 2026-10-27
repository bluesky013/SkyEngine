## 1. 环境前置（Ubuntu 24.04 宿主机）

- [x] 1.1 Python 依赖经 `venv/` 提供（GitPython + ninja；系统 PATH 无 ninja）
- [x] 1.2 系统依赖：`X11/Xlib.h`、`X11/Xutil.h`、`X11/keysym.h`、`X11/Xatom.h`、`xcb/xcb.h`、`libX11.so`、`libxcb.so` 已具备；缺 `X11/Xlib-xcb.h` 与 `libX11-xcb.so` dev 符号链接（无 sudo 安装）→ 以运行期 `dlopen("libX11-xcb.so.1")` 规避
- [ ] 1.3 （可选）`qtbase5-dev`、`libvulkan-dev`：编辑器/后续 RHI 需要，当前不依赖
- [x] 1.4 `python3 python/third_party.py -l` 可列出包

## 2. 三方引导（`third_party.py -p Linux -o build_3rd`）

- [x] 2.1 `python/third_party.py` 新增 `options_<platform>` 条件合并（覆盖在 `options` 之上；无该字段行为不变）
- [x] 2.2 slang：`options_Linux: SLANG_ENABLE_DXIL=OFF`（Linux 走 SPIR-V，不触发 dxc）
- [x] 2.3 slang 在 Linux 构建成功（rebase 后上游升到 `v2026.19` 并改为静态链接：产出 `libslang-compiler.a` 等；其新增 `external/fast_float` 子模块，网络失败时需手动 `git submodule update --init` 后重试）
- [x] 2.4 上游已用 Jolt 替换 Bullet（移除 `bullet3` 包与插件）；Jolt `v5.6.0` 在 Linux 用 `options_Linux: { CMAKE_POSITION_INDEPENDENT_CODE=ON, CPP_RTTI_ENABLED=ON }`（插件子类 `PairFilter : JPH::GroupFilter` 需要基类 typeinfo，否则运行期 dlopen 报 undefined symbol）
- [x] 2.5 recast/enet：`options_Linux: CMAKE_POSITION_INDEPENDENT_CODE=ON`（静态库被链接进共享插件）
- [x] 2.6 `FindSlang.cmake → Findslang.cmake`（`find_package(slang)` 大小写）
- [x] 2.7 `Findsfmt.cmake` 链接名 `SFMT`（产物 `libSFMT.a`）
- [x] 2.8 引擎所需的 Linux 包全部构建成功：sfmt/rapidjson/miniaudio/googletest/vulkan_headers/vma/recast/enet/jolt/lz4/zlib/tracy/freetype/slang/acl/openssl
- [ ] 2.9 cpython 在 Linux 构建失败（`build_cpython.py` exit 2）；默认 `SKY_BUILD_PYTHON=OFF` 不阻塞，启用 python 插件前需修复
- [x] 2.10 `build_3rd/thirdparty_cache_Linux.cmake` 生成

## 3. core 平台层

- [x] 3.1 `DynamicModule.cpp`：Linux `DYN_PREFIX="lib"` / `DYN_SUFFIX=".so"`（`GetLastError` 上游已有 `dlerror` 分支）
- [x] 3.2 `Thread.cpp`：`pthread_setname_np` 填实 `SetCurrentThreadName`（截断 15 字节，`__APPLE__` 签名区分）
- [x] 3.3 `Semaphore`：上游已改为 C++20 `std::counting_semaphore`，Linux 直接可用（无需改）
- [x] 3.4 `engine/core/CMakeLists.txt`：Linux 分支 glob `platform/unix/*` + `${CMAKE_DL_LIBS}`
- [x] 3.5 Linux 下 `Core` 编译链接通过

## 4. framework 原生平台层（无 SDL）

- [x] 4.1 `LinuxPlatform : PlatformBase`：X11 事件循环、steady-clock 计时、`/proc/self/exe` 路径、用户配置路径、`getenv`、`popen`、进程内剪贴板；无显示时仍允许 headless 初始化
- [x] 4.2 `LinuxWindow : NativeWindow`：X11 窗口创建/映射/标题/位置/尺寸、事件映射（Configure/Focus/Key/Button/Motion/ClientMessage）、光标、指针捕获、多窗口、主窗口判定
- [x] 4.3 XID 作为引擎 `WindowID`；`LinuxPlatform::PollEvent` 经 `NativeWindowManager` 分发到对应 `LinuxWindow`
- [x] 4.4 `XcbWindowHandle` 定义在 `engine/core/include/core/platform/NativeWindowHandle.h`；`LinuxWindow` 经 `XGetXCBConnection`（`dlopen`）组装并持有
- [x] 4.5 `engine/framework/CMakeLists.txt` Linux 分支：`platform/linux/*` + `platform/posix/*`，链接 `X11`、`${CMAKE_DL_LIBS}`
- [x] 4.6 `ModuleManager.h` 补 `<memory>`（GCC 暴露的缺失 include）

## 5. project-owned Launcher

- [x] 5.1 新增 `engine/templates/launcher/linux/main.cpp`（Platform Init + GameApplication/XR/Editor 模式）
- [x] 5.2 新增 `engine/templates/launcher/linux/CMakeLists.txt`（`sky_add_exe(TARGET Launcher)`，`OUTPUT_NAME {{PROJECT_NAME}}`，`$ORIGIN` rpath）
- [x] 5.3 `engine/templates/game/native/CMakeLists.txt` 增加 Linux 分支
- [x] 5.4 `python/package_project.py` 的 `LAUNCHER_PLATFORMS` 加 `linux`
- [x] 5.5 移除旧 `engine/launcher`（上游已删除）
- [x] 5.6 `engine/templates/game/export_presets.json` 增加 `{{PROJECT_NAME}}-linux` 导出预设
- [x] 5.7 说明：项目非 develop 构建的 Deploy 目标要求已 cook 的 shader distribution（与 Windows/macOS 相同）；直接验证用 `SKY_DEVELOP=ON`（staging 仅部署 configs）。`package_project.py` 的 generate/package 目前仅实现 android/apple，Linux 打包流程列为后续

## 6. 构建配置与可移植性

- [x] 6.1 `cmake/configuration.cmake` Linux：`dl`/pthread、`CMAKE_POSITION_INDEPENDENT_CODE=ON`、`$ORIGIN` rpath
- [x] 6.2 `plugins/recast/src/RecastTileGenerator.cpp` 补 `<cstring>`
- [x] 6.2b `engine/framework/src/world/WorldStreaming.cpp` 泛型 lambda 中的依赖模板调用补 `template` 关键字（GCC 要求 `actor->template GetComponent<T>()`）
- [x] 6.3 `Flags.h`/`TypeId.h` 的 GCC 修复（rebase 前落地，保留）
- [ ] 6.4 `configs/asset_build_presets.json` 加 `linux` preset（真实 cook 待 builder 适配）

## 7. 验证

- [x] 7.1 engine 根工程 configure + build 通过（Ninja/Release，`3RD_PATH=build_3rd/Linux`；基线 `ba105c59`）
- [x] 7.2 用 `package_project.py create` 生成示例工程（含 `native/linux`），`linux` preset + `SKY_DEVELOP=ON` configure + build 通过
- [x] 7.3 运行示例 `SkyLinuxTest`：原生 X11 窗口 + 事件循环正常；Aurora.Metadata/Aurora.Render 模块加载、Vulkan instance/device 初始化成功
- [x] 7.4 当前止步于 `no platform surface support compiled in`（WSI 未实现，见 8）
- [ ] 7.5 `SKY_BUILD_TEST=ON` + `ctest`（Linux headless 用例）
- [ ] 7.6 Win32/macOS 回归确认（由对应平台跟进）

## 8. aurora Vulkan xcb WSI（**暂缓，待接显示器统一处理**）

- [ ] 8.1 `VulkanInstance`：Linux 加 `VK_KHR_surface` + `VK_KHR_xcb_surface`
- [ ] 8.2 `VulkanFunctions`：`vkCreateXcbSurfaceKHR` 声明/加载
- [ ] 8.3 `VulkanSwapChain::CreateSurface`：xcb 分支（`XcbWindowHandle*` → `vkCreateXcbSurfaceKHR`）
- [ ] 8.4 `engine/aurora/rhi/vulkan/CMakeLists.txt`：`VK_USE_PLATFORM_XCB_KHR` + `xcb`/`X11-xcb`
- [ ] 8.5 clear-screen 端到端（X11 实机 + xvfb/lavapipe）
- [ ] 8.6 Wayland 会话行为（明确报错，列入后续 change）

## 9. 文档、CI 与收尾

- [ ] 9.1 README/AGENTS 补 Linux 构建前置与 `third_party.py -p Linux -o build_3rd` 工作流
- [ ] 9.2 GitHub Actions Linux job（third_party 缓存 + Ninja + xvfb + lavapipe）
- [ ] 9.3 archive 本 change（archive 前需用户确认）
