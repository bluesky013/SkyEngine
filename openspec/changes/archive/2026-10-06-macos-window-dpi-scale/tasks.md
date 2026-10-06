# Tasks: macos-window-dpi-scale

## 1. 实现

- [x] 1.1 `CocoaWindow.h` 声明 `float GetDpiScale() const override`
- [x] 1.2 `CocoaWindow.mm` 实现：无窗口返回缓存 `scale`，否则读 `NSWindow.backingScaleFactor`

## 2. 验证

- [x] 2.1 独立语法编译通过（`CocoaWindow.mm`）
- [x] 2.2 `Framework` / `Launcher` 构建通过
