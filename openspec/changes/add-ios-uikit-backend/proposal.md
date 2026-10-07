# Change: add-ios-uikit-backend

> 状态：proposal（未实现）。聚焦 iOS 的 **UIKit 窗口/事件后端**实现；是 `add-ios-platform`（伞形：后端 + 启动器 + 三方 + 验证）的后端细节补充。

## Why

`add-ios-platform` 已定 iOS 运行时支持的范围，但后端细节（UIKit 引导、视图/图层、事件循环驱动、触摸/软键盘映射、生命周期）需要在实现前敲定并可落地验证。本 change 只做 **`engine/framework/platform/ios` 的 UIKit 后端**，与 macOS 的 Cocoa 后端结构对齐（AppKit 类型封装在 `.mm`、句柄走 `void*`、ARC）。

现状要点：
- `engine/core` 已有 `SKY_PLATFORM_IOS` 与 `PlatformType::IOS`；
- `engine/framework` 的 `Darwin` 分支现编译 `platform/macos/*`（AppKit `NSApplication/NSWindow`）→ iOS 不可用；
- 本机有 iOS SDK（Xcode），可对 **模拟器**做构建/运行验证。

## What Changes

- 新增 `engine/framework/platform/ios/`：
  - `IOSPlatform : PlatformBase`：生命周期、路径、计时；`PollEvent` 与 **UIKit 事件循环**协同。
  - `IOSWindow : NativeWindow`：`UIViewController`（`UIView`，`+layerClass = CAMetalLayer`）；`GetNativeHandle()` 返回该 `CAMetalLayer`；`GetDpiScale()` 取 `UIScreen`。
- **事件**：`touchesBegan/Moved/Ended/Cancelled` → 引擎输入事件；软键盘（`UIKeyInput`/`UIResponder`）→ `IKeyboardEvent`（`OnKeyDown/Up`、`OnTextInput`）。
- **帧驱动**：`CADisplayLink` 驱动引擎帧（替代 `NSApplication` 循环），与 `Application::Loop` 协同。
- **生命周期**：`applicationWillResignActive`/`didEnterBackground`/`willEnterForeground` → 暂停/恢复，避免 drawable 泄漏。
- **构建**：`engine/framework/CMakeLists.txt` 增加 iOS 分支（`CMAKE_SYSTEM_NAME STREQUAL "iOS"` → `platform/ios/*`，链 `UIKit/QuartzCore/Metal/Foundation`）；iOS `.mm` 启用 ARC。
- 启动器（`main.mm`/`AppDelegate`）属 `add-ios-platform`，本 change 依赖其提供入口。

## Capabilities

### New Capabilities
- `ios-uikit-backend`: UIKit 窗口/事件后端（视图+`CAMetalLayer`、`CADisplayLink` 帧驱动、触摸/软键盘事件、生命周期、iOS CMake 分支）。

## Impact

- 新增 `engine/framework/platform/ios/**`
- `engine/framework/CMakeLists.txt`（iOS 分支）
- `engine/framework/src/application/Application.*` / `GameApplication`（`Mainloop` seam 与初始化顺序：iOS 由 `CADisplayLink` 驱动帧，不走桌面 `PollEvent` 循环）
- 依赖：`native-platform-backend`（接口）、`add-ios-platform`（启动器/三方/验证）、`aurora-rhi-metal`（渲染到 `CAMetalLayer`）
