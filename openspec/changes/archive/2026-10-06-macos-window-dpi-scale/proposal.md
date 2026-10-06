# Change: macos-window-dpi-scale

## Why

`NativeWindow::GetDpiScale()` 的 macOS 实现缺失：`CocoaWindow` 设置了 `scale` 成员，但没有 override 该方法，于是始终返回 `NativeWindow` 的默认值 `1.0f`（Windows 后端 `Win32Window` 已实现）。retina 显示器下 DPI 错误，直接消费者 `engine/sandbox/render/src/EditorRenderer.cpp`（`surface->dpiScale = surface->window->GetDpiScale()`）拿到 1.0，编辑视口分辨率与命中测试随之错误。

## What Changes

- `CocoaWindow` 新增 `GetDpiScale()` override：无窗口时返回缓存 `scale`，否则实时读取 `NSWindow.backingScaleFactor`。

实时读取（而非缓存）使窗口在**不同缩放比的显示器之间移动**时自动返回正确值，无需额外的屏幕变化通知。

## Capabilities

### Modified Capabilities

- `native-platform-backend`: 新增「窗口 DPI 缩放」要求——后端的 `GetDpiScale()` 必须返回设备像素 / point 比例，且反映当前所在显示器。

## Impact

- `engine/framework/platform/macos/CocoaWindow.{h,mm}`
- 无接口变更；Windows 后端已有实现，Sandbox 消费者自动修正
