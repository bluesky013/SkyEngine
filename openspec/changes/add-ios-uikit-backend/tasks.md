# Tasks: add-ios-uikit-backend

## 1. Platform / window

- [ ] 1.1 `engine/framework/platform/ios/IOSPlatform.{h,mm}`：`PlatformBase`（路径/计时/`GetType()==IOS`）；`PollEvent` 与 UIKit 事件回调协同
- [ ] 1.2 `engine/framework/platform/ios/IOSWindow.{h,mm}`：`UIViewController`+`SkyIOSView`（`+layerClass = CAMetalLayer`）；`GetNativeHandle()` 返回 `CAMetalLayer`；`GetDpiScale()` = `UIScreen` scale
- [ ] 1.3 `NativeWindow::Create` 的 iOS 实现（`SKY_PLATFORM_IOS` 守卫）

## 2. Frame / lifecycle

- [ ] 2.1 `CADisplayLink` 驱动引擎帧（与 `Application::Loop` 的 seam）
- [ ] 2.2 前后台：`applicationWillResignActive`/`didEnterBackground`/`willEnterForeground` → 暂停/恢复，停止 display link 避免 drawable 泄漏

## 3. Input

- [ ] 3.1 触摸 `touchesBegan/Moved/Ended/Cancelled` → `Mouse*`（坐标按 scale 换算）
- [ ] 3.2 软键盘 `UIKeyInput`/`UIResponder` → `IKeyboardEvent`（`OnKeyDown/Up`、`OnTextInput`）

## 4. Build

- [ ] 4.1 `engine/framework/CMakeLists.txt`：iOS 分支（`platform/ios/*` + UIKit/QuartzCore/Metal/Foundation），macOS 分支仅 `platform/macos/*`
- [ ] 4.2 iOS `.mm` ARC

## 5. Verification

- [ ] 5.1 iOS 模拟器构建（需 iOS 三方，见 `add-ios-platform`）
- [ ] 5.2 窗口创建 + 渲染一帧（`Aurora.Metal` → `CAMetalLayer`）
- [ ] 5.3 触摸/软键盘事件冒泡；前后台切换无泄漏
