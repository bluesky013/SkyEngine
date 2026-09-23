# Change: framework-native-platform-macos

## Why

`framework-native-platform` 落了 Win32 原生后端，macOS 仍走 `SDLPlatform`（`MacosPlatform : SDLPlatform`）。macOS 需要与 Win32 对齐的原生窗口/事件后端，且 Metal swapchain 与 MoltenVK surface 都要求窗口 handle 是 `CAMetalLayer`——SDL 窗口无法满足。

## What Changes

- **CocoaWindow**（`platform/macos/CocoaWindow.{h,mm}`）：`NativeWindow` 子类。创建 NSWindow（titled/closable/miniaturizable/resizable）+ `SkyCocoaView`（`makeBackingLayer` 返回 `CAMetalLayer`，`wantsLayer=YES`）；`GetNativeHandle()` 返回该 CAMetalLayer。事件映射对齐 Win32 覆盖范围：
  - 键盘：macOS virtual keycode → `ScanCode`（US ANSI 全表 + keypad + F1-F12）；modifierFlags → `KeyMod`（AppKit 不区分左右，左右位同时置位）；`keyDown` 的 `characters`（滤掉控制字符）→ `OnTextInput`
  - 鼠标：down/up（left/right/middle + clickCount）、move/drag（tracking area + `acceptsMouseMovedEvents`；rel 来自 `deltaX/deltaY`）、wheel（`scrollingDeltaY`）；坐标转换为 view 坐标并翻转到左上角原点
  - 窗口：`setFrameSize:` / `viewDidChangeBackingProperties` → `OnWindowResize`（backing 像素尺寸，含 retina 迁移）；`windowDidBecomeKey/ResignKey` → `OnFocusChanged`；`windowWillClose` → 主窗口关闭时请求退出主循环（对齐 WM_QUIT 语义）
  - layer `contentsScale` 跟随 `backingScaleFactor`；`WindowID` 取 `windowNumber`
- **MacosPlatform**：基类由 `SDLPlatform` 改为 `PlatformBase`。`Init` 预建 NSApplication（activation policy + `finishLaunching` + 含 Cmd+Q 的最小菜单）；`PollEvent` 非阻塞泵 `nextEventMatchingMask` + `updateWindows`；mach_absolute_time 计时；NSPasteboard 剪贴板；`NSApplicationSupportDirectory/SkyEngine/SkyEditor` 用户配置路径；getenv/popen；NSOpenPanel/NSSavePanel 文件对话框；`SetMainWindow`/`GetMainWinHandle` 返回主窗口 CAMetalLayer。
- **构建**：Darwin 只编译 `platform/macos/*`，链接 `Cocoa`/`QuartzCore` framework，不再链接 SDL（SDL 仍保留在树中供其他平台/回退）。

## Capabilities

### Modified Capabilities

- `native-platform-backend`: macOS 后端落地；macOS 的 RHI surface handle 明确为 `CAMetalLayer` 本身（非 layer-backed view），与 Metal swapchain 和 MoltenVK `VK_EXT_metal_surface` 的消费方式一致。

## Impact

- `engine/framework/platform/macos/`：MacosPlatform 重写 + 新增 CocoaWindow
- `engine/framework/CMakeLists.txt`：Darwin 源集与链接库
- `engine/launcher/macos/MacosLauncher.mm` 等上层零改动（接口不变）
