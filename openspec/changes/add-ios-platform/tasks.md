## 1. Platform backend (UIKit)

- [ ] 1.1 `IOSPlatform : PlatformBase`（`platform/ios`）：`PollEvent` 泵 UIKit 运行循环、计时、路径、`GetType()==IOS`
- [ ] 1.2 `IOSWindow : NativeWindow`：`UIViewController`/`UIView`（`layerClass = CAMetalLayer`），`GetNativeHandle()` 返回 `CAMetalLayer`，`GetDpiScale()` 取 `UIScreen` scale
- [ ] 1.3 事件映射：触摸 → `Mouse*`（或新增触摸事件）；软键盘 `UIKeyInput` → `IKeyboardEvent`
- [ ] 1.4 生命周期：`applicationWillResignActive`/`didBecomeActive` 处理暂停/恢复

## 2. Build

- [ ] 2.1 `engine/framework/CMakeLists.txt`：Darwin/iOS 分支拆分（`platform/macos` vs `platform/ios`），iOS 链接 UIKit/QuartzCore/Metal/Foundation
- [ ] 2.2 iOS `.mm` 启用 ARC（与 Metal 后端一致）
- [ ] 2.3 三方：为运行期包补齐 `IOS`（或记录经 Xcode 交叉构建的 `3RD_PATH`）

## 3. Launcher

- [ ] 3.1 `engine/launcher/ios/`：`main.mm`（`UIApplicationMain`）+ AppDelegate/ViewController 承载引擎视图
- [ ] 3.2 从应用生命周期启动引擎 `Application`

## 4. RHI

- [ ] 4.1 `Aurora.Metal` 在 iOS 构建并渲染到 `CAMetalLayer`
- [ ] 4.2 旋转/尺寸变化下的 swapchain/resize 验证

## 5. Verification

- [ ] 5.1 模拟器/真机：窗口创建、事件、渲染一帧
- [ ] 5.2 触控/软键盘事件冒泡到引擎
- [ ] 5.3 背景/前台切换无 drawable 泄漏
