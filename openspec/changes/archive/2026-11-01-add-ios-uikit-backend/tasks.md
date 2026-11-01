# Tasks: add-ios-uikit-backend

> Status (2026-11-01): **superseded**；仅建议 history-only 归档，**不同步旧 delta**。输入/scene-host 剩余验收归 `add-harmonyos-platform` 3.10；GPU 生命周期归 `aurora-renderer-platform-parity` P02-IOS。归档仍待用户确认。

## Current disposition and transferred acceptance — 2026-11-01

The checklist below is retained as a historical proposal record, not a second active implementation backlog. Existing UIKit/ARC, external frame loop, scene manifest, shipping products and basic Metal rendering are delivered by `complete-native-platform-services`, `verify-ios-world-packaging` and `fix-ios-navigation-demo-linkage`. Historical unchecked boxes are not bulk-marked complete.

- Input and scene-host acceptance: `add-harmonyos-platform` task 3.10 owns real touch/IME/native text UI, cancellation/reopen, scene identity/callback and frame-epoch checks.
- GPU lifecycle acceptance: `aurora-renderer-platform-parity` task P02-IOS owns physical-device rotation/resize, background/foreground, Surface replacement, exact-generation retirement, failure recovery and sustained GPU/present validation.
- The real iPad navigation scene proves basic signed deployment, Metal presentation and asynchronous routing. It does not close either transferred matrix. Archive this old proposal only as history, without syncing its obsolete delta, after user approval.

## 1. Platform / window

- [x] 1.1 `engine/framework/platform/ios/IOSPlatform.{h,mm}`：`PlatformBase`（路径/计时/`GetType()==IOS`）；`PollEvent` 与 UIKit 事件回调协同
- [x] 1.2 `engine/framework/platform/ios/IOSWindow.{h,mm}`：`UIViewController`+`SkyIOSView`（`+layerClass = CAMetalLayer`）；`GetNativeHandle()` 返回 `CAMetalLayer`；`GetDpiScale()` = `UIScreen` scale
- [x] 1.3 `NativeWindow::Create` 的 iOS 实现（`SKY_PLATFORM_IOS` 守卫）

## 2. Frame / lifecycle

- [ ] 2.1 `Application` 的 **Mainloop seam**：iOS 不走桌面 `PollEvent` 循环；`CADisplayLink` tick 直接驱动每帧入口（`Loop`/`Tick` hook）
- [x] 2.2 初始化顺序：AppDelegate 先建 `UIViewController`/`SkyIOSView`，`NativeWindow::Create`/`GameApplication::PreInit` 在其后（不可先建窗口）
- [ ] 2.3 前后台：`applicationWillResignActive`/`didEnterBackground`/`willEnterForeground` → 暂停/恢复，停止 display link 避免 drawable 泄漏

## 3. Input

- [x] 3.1 触摸 `touchesBegan/Moved/Ended/Cancelled` → `Mouse*`（坐标按 scale 换算）
- [x] 3.2 软键盘 `UIKeyInput`/`UIResponder` → `IKeyboardEvent`（`OnKeyDown/Up`、`OnTextInput`）

## 4. Build

- [x] 4.1 `engine/framework/CMakeLists.txt`：iOS 分支（`platform/ios/*` + UIKit/QuartzCore/Metal/Foundation），macOS 分支仅 `platform/macos/*`
- [x] 4.2 iOS `.mm` ARC

## 5. Verification

- [ ] 5.1 iOS 模拟器构建（需 iOS 三方，见 `add-ios-platform`）
- [ ] 5.2 窗口创建 + 渲染一帧（`Aurora.Metal` → `CAMetalLayer`）
- [ ] 5.3 触摸/软键盘事件冒泡；前后台切换无泄漏
