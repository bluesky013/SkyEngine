# Design: framework-native-platform-macos

## 结构对齐 Win32

- `CocoaWindow : NativeWindow` 对应 `Win32Window`；`MacosPlatform : PlatformBase` 对应 `Win32Platform`。接口层（`Platform` / `NativeWindow` / `IWindowEvent`）零改动，launcher/editor host 不需要修改。
- AppKit 类型不进头文件（同 Win32 的 windows.h 隔离策略），全部 `void*` 放在 .mm。

## surface handle = CAMetalLayer

Metal swapchain（`MetalSwapChain::Init` 把 `desc.window` 当 `CAMetalLayer*`）与 Vulkan/MoltenVK macOS 路径（`VK_EXT_metal_surface` 的 `pLayer`）都直接消费 CAMetalLayer。因此 `CocoaWindow` 的 content view 用 `makeBackingLayer` 产出 CAMetalLayer，`GetNativeHandle()` 返回该 layer（而非 NSView/NSWindow）。主 spec 中「CAMetalLayer-backed view」的表述相应修正。

## 事件映射要点

- **坐标系**：Cocoa 为左下角原点，引擎约定左上角；view 内坐标统一翻转 Y。resize 事件用 `convertRectToBacking` 的像素尺寸（swapchain drawableSize 需要像素）。
- **修饰键**：AppKit modifierFlags 不区分左右；左右位同时置位，保证 `KeyMod::CTRL`/`SHIFT` 等组合判断与 Win32 行为一致。
- **键盘 repeat**：`keyDown` 自动重复事件原样广播（与 Win32 WM_KEYDOWN repeat 一致）；text input 取 `characters` 并滤掉 < 0x20 的控制字符（对齐 WM_CHAR 行为）。
- **retina 迁移**：`viewDidChangeBackingProperties` 更新 layer contentsScale 并重新广播 resize。

## 生命周期与退出

- NSWindow `releasedWhenClosed=NO`，由 `CocoaWindow` 析构显式 close+release；delegate 由 window 对象持有所有权（NSWindow 不 retain delegate）。
- 退出语义对齐 Win32 的 WM_QUIT：主窗口 `windowWillClose` → `MacosPlatform::NotifyWindowClosed` → `PollEvent` 置 exit。Cmd+Q 经最小应用菜单 → `terminate:`。

## 内存管理

工程 .mm 为 MRC（Metal 后端同）；显式 retain/release，事件泵包 `@autoreleasepool`。
