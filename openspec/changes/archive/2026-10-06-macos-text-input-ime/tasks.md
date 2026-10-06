# Tasks: macos-text-input-ime

## 1. NSTextInputClient

- [x] 1.1 `SkyCocoaView` 声明 `<NSTextInputClient>` + marked-text ivar（MRC，dealloc 释放）
- [x] 1.2 `keyDown` 广播 `OnKeyDown` 后走 `interpretKeyEvents:`
- [x] 1.3 实现 `hasMarkedText` / `markedRange` / `selectedRange` / `setMarkedText:selectedRange:replacementRange:` / `unmarkText` / `validAttributesForMarkedText` / `attributedSubstringForProposedRange:actualRange:` / `insertText:replacementRange:` / `characterIndexForPoint:` / `firstRectForCharacterRange:actualRange:` / `doCommandBySelector:`
- [x] 1.4 `insertText:` 先 unmark 再以 UTF-8 广播 `OnTextInput`（空串忽略）
- [x] 1.5 失焦（`windowDidResignKey`）丢弃 marked text

## 2. 验证

- [x] 2.1 独立语法编译通过
- [x] 2.2 `Framework` / `Launcher` 构建通过 + `-r metal` 冒烟运行
- [ ] 2.3 手动验证 CJK 组合输入上屏（需人工用输入法；自动化不可行）
