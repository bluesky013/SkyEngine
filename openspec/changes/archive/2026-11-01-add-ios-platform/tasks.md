# Tasks: add-ios-platform

> Status (2026-11-01): **superseded**；仅建议 history-only 归档，**不同步旧 delta**。输入/scene-host 剩余验收归 `add-harmonyos-platform` 3.10；GPU 生命周期归 `aurora-renderer-platform-parity` P02-IOS。归档仍待用户确认。

## Current disposition and transferred acceptance — 2026-11-01

The checklist below is retained as a historical proposal record, not a second active implementation backlog. Existing UIKit/ARC, external frame loop, scene manifest, shipping products and basic Metal rendering are delivered by `complete-native-platform-services`, `verify-ios-world-packaging` and `fix-ios-navigation-demo-linkage`. Historical unchecked boxes are not bulk-marked complete.

- Input and scene-host acceptance: `add-harmonyos-platform` task 3.10 owns real touch/IME/native text UI, cancellation/reopen, scene identity/callback and frame-epoch checks.
- GPU lifecycle acceptance: `aurora-renderer-platform-parity` task P02-IOS owns physical-device rotation/resize, background/foreground, Surface replacement, exact-generation retirement, failure recovery and sustained GPU/present validation.
- The real iPad navigation scene proves basic signed deployment, Metal presentation and asynchronous routing. It does not close either transferred matrix. Archive this old proposal only as history, without syncing its obsolete delta, after user approval.

> 伞形 change：本 change 只保留 iOS 运行时支撑中**非后端**的部分（启动器/三方/RHI/验证）。
> UIKit 窗口/事件后端与框架 CMake 分支由 `add-ios-uikit-backend` 实现（tasks 1.x）。

## 1. UIKit backend（delegated）

- 见 `openspec/changes/archive/2026-11-01-add-ios-uikit-backend/`（`IOSPlatform`/`IOSWindow`、事件、`CADisplayLink`、CMake iOS 分支、ARC）

## 2. Third-party

- [ ] 2.1 为运行期包补齐 `IOS` 平台（`cmake/thirdparty.json`），或记录经 Xcode 交叉构建的 `3RD_PATH`（`python/third_party.py -p IOS`）
- [ ] 2.2 iOS 构建**关闭 editor/cook**（`SKY_BUILD_SANDBOX=OFF` 且不构建 cook）：assimp/meshoptimizer/stb/nanosvg/astc/ispc_texcomp/GKlib/metis 等仅 Win/Mac 声明，运行期不需要

## 3. Launcher

- [ ] 3.1 `engine/templates/launcher/ios/`：`main.mm`（`UIApplicationMain`）+ AppDelegate/ViewController 承载引擎视图
- [ ] 3.2 从应用生命周期启动引擎 `Application`（与 `add-ios-uikit-backend` 的 `CADisplayLink` 帧循环协同）
- [ ] 3.3 **Info.plist / app bundle**：bundle id、显示名、支持的设备朝向、`Metal`/`UIRequiredDeviceCapabilities` 等；随 `.app` 打包（bundle id 定制见 `add-game-project-build` 8.x）


## 4. RHI

- [ ] 4.1 `Aurora.Metal` 在 iOS 构建并渲染到 `CAMetalLayer`
- [ ] 4.2 旋转/尺寸变化下的 swapchain/resize 验证

## 5. Verification

- [ ] 5.1 模拟器/真机：窗口创建、事件、渲染一帧
- [ ] 5.2 触控/软键盘事件冒泡到引擎
- [ ] 5.3 背景/前台切换无 drawable 泄漏
