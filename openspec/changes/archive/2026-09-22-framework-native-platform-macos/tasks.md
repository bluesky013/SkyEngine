# Tasks: framework-native-platform-macos

## 1. CocoaWindow

- [x] 1.1 `CocoaWindow.{h,mm}`：NSWindow + CAMetalLayer-backed `SkyCocoaView`（makeBackingLayer/wantsLayer），`GetNativeHandle()` 返回 CAMetalLayer
- [x] 1.2 键盘：keycode→ScanCode 全表（US ANSI + keypad + F1-F12），modifierFlags→KeyMod（左右同置），`characters`→OnTextInput（滤控制字符）
- [x] 1.3 鼠标：button down/up（clickCount）、motion（tracking area + delta）、wheel；view 坐标翻转 Y
- [x] 1.4 窗口：setFrameSize/viewDidChangeBackingProperties→OnWindowResize（backing 像素）；become/resign key→OnFocusChanged；willClose→退出请求
- [x] 1.5 `NativeWindow::Create` macOS 实现；WindowID=windowNumber；注册 NativeWindowManager

## 2. MacosPlatform

- [x] 2.1 基类改为 PlatformBase；`Init` 预建 NSApplication + 最小菜单（Cmd+Q）
- [x] 2.2 `PollEvent` 非阻塞 NSEvent 泵 + updateWindows + exitRequested
- [x] 2.3 mach_absolute_time 计时；NSPasteboard 剪贴板；getenv；popen
- [x] 2.4 路径：Internal/Bundle 保持原语义；UserConfigPath=ApplicationSupport/SkyEngine/SkyEditor
- [x] 2.5 NSOpenPanel/NSSavePanel 文件对话框（filter 解析 "*.-ext;..."）
- [x] 2.6 SetMainWindow/GetMainWinHandle（CAMetalLayer）；NotifyWindowClosed

## 3. 构建

- [x] 3.1 Darwin 只编译 platform/macos/*，链接 Cocoa/QuartzCore，不再链接 SDL

## 4. spec 与验证

- [x] 4.1 `native-platform-backend` 主 spec 同步（macOS handle=CAMetalLayer）
- [x] 4.2 新文件独立语法编译验证（完整构建受 origin 新增第三方 acl/miniaudio 阻塞，待第三方补齐后回归）
