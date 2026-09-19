## Why

There is no text-entry widget and no text-input event routing, and pointer capture is single-pointer only.
Text entry, IME text, and multi-touch are core, render-independent input capabilities.

## What Changes

- Add pointer ids to pointer events and make capture per-pointer (multi-touch capture).
- Route committed text input (IME) to the focused element, with bubbling.
- Focus the nearest focusable ancestor on pointer down.
- Add `EditBox`: a focusable text field with a caret, key handling (backspace/left/right), and text insertion.

**Non-goals**: selection, clipboard, cursor blink animation, soft-keyboard integration, and gestures.

## Capabilities

### New Capabilities
- `ui-input`: text entry, IME text routing, and multi-pointer capture.

## Impact

- `UIEvent.h` (pointer id), `UIElement.{h,cpp}` (text-input hook), `UIEventRouter.{h,cpp}` (per-pointer capture,
  focus-on-down, text dispatch), new `widgets/EditBox.{h,cpp}`, registry entry; tests in `engine/test/ui`.
