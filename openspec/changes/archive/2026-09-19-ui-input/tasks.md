## 1. Event model

- [x] 1.1 Add `pointerId` to `UIPointerEvent`
- [x] 1.2 Add `UIElement::OnTextInput`
- [x] 1.3 Per-pointer capture, focus-on-down, and text dispatch in `UIEventRouter`

## 2. EditBox

- [x] 2.1 Add `widgets/EditBox.{h,cpp}` (text, caret, key/text handling, theme paint) and register `EditBox`

## 3. Tests

- [x] 3.1 Tests: focus on press, typing via text input, backspace, caret move, multi-pointer independent capture
- [x] 3.2 Build and run the suite green
