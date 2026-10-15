# Change: add-ios-platform

> 历史提案，保留未归档。下文现状描述属于提案时点，不代表当前仓库。
> UIKit 平台、launcher、UIScene 和 unsigned IPA 的后续实现/验证见
> [complete-native-platform-services](../archive/2026-10-16-complete-native-platform-services/validation.md)。
> 本提案的 GPU 真机、完整分发等范围不能由该结果推定完成；原 tasks 未自动勾选，未执行 archive。

## Why

Apple 平台目前**只有 macOS 完整支持**，iOS 处于「仅核心识别」状态：

- 核心已有 `SKY_PLATFORM_IOS` 与 `PlatformType::IOS`（`engine/core/include/core/platform/Platform.h`），三方 bootstrap 也有 `IOS` 平台（ios-cmake 工具链，见 `cmake/thirdparty.json` 首个包）。
- 但**没有 iOS 的窗口/事件后端**：`engine/framework/CMakeLists.txt` 的 `Darwin` 分支使用 `platform/macos/*`，其中 `CocoaWindow`/`MacosPlatform` 基于 AppKit（`NSApplication/NSWindow`），在 iOS 不可用。
- **没有 iOS 启动器**：`engine/templates/launcher/` 只有 `android/macos/windows`。
- 运行时三方包基本未声明 `IOS`（仅 `ios-cmake` 工具链）。
- `Aurora.Metal` 的门控是 `APPLE`（会尝试为 iOS 构建），但窗口来源是 AppKit → 破坏。

结果：iOS 既不能构建出可运行的应用，RHI 也无法获得合法的表面句柄。

## What Changes

- **iOS 平台层（委托 `add-ios-uikit-backend`）**：UIKit 窗口/事件后端（`IOSPlatform`/`IOSWindow`、`CAMetalLayer` 句柄、`CADisplayLink` 帧驱动、触摸/软键盘、生命周期、框架 CMake iOS 分支/ARC）由 `add-ios-uikit-backend` 实现；本 change 只保留其**表面契约**（`GetNativeHandle()` 返回 `CAMetalLayer`）与其余 iOS 支撑。
- **启动器**：新增 `engine/templates/launcher/ios`（工程 launcher 模板，随 `package_project.py create` 铺到 `native/ios`）（`main.mm` + AppDelegate 承载 view），复用 `Application`/模块加载。
- **三方集**：为运行期包补齐 `IOS` 平台（或说明经 Xcode 交叉构建的路径）；`-D3RD_PATH` 指向 iOS 产物。
- **RHI 验证**：`Aurora.Metal` 在 iOS 上构建并渲染到 `CAMetalLayer`；swapchain/RDG 在移动端可用。

## Capabilities

### Modified Capabilities

- `native-platform-backend`: 新增 iOS **RHI 表面契约**——`GetNativeHandle()` 返回 `CAMetalLayer`（后端细节见 `ios-uikit-backend`）。

## Impact

- 新增 `engine/templates/launcher/ios/**`；UIKit 后端 `engine/framework/platform/ios/**` 由 `add-ios-uikit-backend` 交付
- `engine/templates/launcher/ios/CMakeLists.txt`
- `cmake/thirdparty.json`（IOS 运行期包）、可能的 iOS 部署/打包脚本
- `Aurora.Metal` 在 iOS 的构建与移动端验证
- 依赖：`add-ios-uikit-backend`（UIKit 后端/CMake）、`native-platform-backend`（接口）、`aurora-rhi-metal`（后端）
