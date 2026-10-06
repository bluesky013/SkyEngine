# Change: macos-text-input-ime

## Why

macOS 窗口此前只把 `keyDown` 的 `event.characters` 直接广播为 `OnTextInput`，没有接入 AppKit 文本系统：

- **CJK/IME 组合输入完全不可用**（无 `NSTextInputClient`，无 marked text / 候选窗口）。
- **Command 快捷键会漏成文本**：`Cmd+C` 之类未被菜单/responder 拦截，会产出 `OnTextInput("c")`。
- 控制字符过滤条件冗余（`first >= 0x20 || first > 0x7F`）。

## What Changes

- `SkyCocoaView` 实现 `NSTextInputClient`：`keyDown` 在广播 `OnKeyDown` 后调用 `interpretKeyEvents:`，文本经 `insertText:replacementRange:` 到达，组合态经 `setMarkedText:`/`unmarkText` 管理；`doCommandBySelector:` 吞掉方向键/删除等特殊键（物理键已由 `OnKeyDown` 送出）；`firstRectForCharacterRange:` 以当前鼠标位置换算屏幕坐标，作为候选窗口锚点。
- 提交文本（含 IME 上屏）经 `OnTextInput` 广播；组合中间态不广播（接口仅有 `OnTextInput`）。
- 窗口失焦（`windowDidResignKey`）时丢弃 in-flight 组合文本。
- 组合输入不再经由 `event.characters`，`Cmd` 组合由菜单/selector 路径处理，消除漏文本。

Win32 侧 IME（`WM_IME_*`/Imm32）仍待后续 change。

## Capabilities

### Modified Capabilities

- `native-platform-backend`: 「Text input and IME」requirement 由「CJK 组合输入留待 later phase」更新为 **macOS 已提供 IME 组合输入**，并明确组合中间态不经 `OnTextInput`、失焦丢弃。

## Impact

- `engine/framework/platform/macos/CocoaWindow.mm`
- 无接口变更；`MacosPlatform` 的 `StartTextInput/StopTextInput/SetTextInputRect` 保持默认空实现（IME 依赖 first responder + `NSTextInputClient`，无需显式启动）
