# Change: add-ios-platform

> 状态：proposal（未实现）。为 iOS 增加运行时平台支持（窗口/事件后端 + 启动器 + 三方集）。

## Why

Apple 平台目前**只有 macOS 完整支持**，iOS 处于「仅核心识别」状态：

- 核心已有 `SKY_PLATFORM_IOS` 与 `PlatformType::IOS`（`engine/core/include/core/platform/Platform.h`），三方 bootstrap 也有 `IOS` 平台（ios-cmake 工具链，见 `cmake/thirdparty.json` 首个包）。
- 但**没有 iOS 的窗口/事件后端**：`engine/framework/CMakeLists.txt` 的 `Darwin` 分支使用 `platform/macos/*`，其中 `CocoaWindow`/`MacosPlatform` 基于 AppKit（`NSApplication/NSWindow`），在 iOS 不可用。
- **没有 iOS 启动器**：`engine/launcher/` 只有 `android/macos/windows`。
- 运行时三方包基本未声明 `IOS`（仅 `ios-cmake` 工具链）。
- `Aurora.Metal` 的门控是 `APPLE`（会尝试为 iOS 构建），但窗口来源是 AppKit → 破坏。

结果：iOS 既不能构建出可运行的应用，RHI 也无法获得合法的表面句柄。

## What Changes

- **iOS 平台层**：新增 `engine/framework/platform/ios`（`IOSPlatform` + `IOSWindow`），基于 UIKit：
  - `UIApplicationMain` + AppDelegate 驱动事件循环（`PollEvent`）；
  - 内容 `UIView` 的 `layerClass` 为 `CAMetalLayer`；`GetNativeHandle()` 返回该 `CAMetalLayer`（与 macOS/Metal swapchain 及 MoltenVK `VK_EXT_metal_surface` 一致）；
  - 触摸映射为引擎事件（按下/抬起/移动 → 现有 `Mouse*`/或新增触摸事件），键盘（软键盘）经 `UIKeyInput`/`UITextInput` 映射到 `IKeyboardEvent`（含 `OnTextInput`）。
- **CMake 拆分**：`Darwin` 分支按 `CMAKE_SYSTEM_NAME` 区分 `macOS`（`platform/macos/*`）与 `iOS`（`platform/ios/*`），并加 iOS 系统 framework（`UIKit`、`QuartzCore`、`Metal`、`Foundation`）。
- **启动器**：新增 `engine/launcher/ios`（`main.mm` + AppDelegate 承载 view），复用 `Application`/模块加载。
- **三方集**：为运行期包补齐 `IOS` 平台（或说明经 Xcode 交叉构建的路径）；`-D3RD_PATH` 指向 iOS 产物。
- **RHI 验证**：`Aurora.Metal` 在 iOS 上构建并渲染到 `CAMetalLayer`；swapchain/RDG 在移动端可用。

## Capabilities

### Modified Capabilities

- `native-platform-backend`: 新增 iOS（UIKit）窗口/事件后端要求——`GetNativeHandle()` 返回 `CAMetalLayer`；事件循环由 `UIApplicationMain` 驱动；触摸/软键盘映射到引擎事件。

## Impact

- 新增 `engine/framework/platform/ios/**`、`engine/launcher/ios/**`
- `engine/framework/CMakeLists.txt`（Darwin/iOS 分支）、`engine/launcher/CMakeLists.txt`
- `cmake/thirdparty.json`（IOS 运行期包）、可能的 iOS 部署/打包脚本
- `Aurora.Metal` 在 iOS 的构建与移动端验证
- 依赖：`native-platform-backend`（接口）、`aurora-rhi-metal`（后端）
