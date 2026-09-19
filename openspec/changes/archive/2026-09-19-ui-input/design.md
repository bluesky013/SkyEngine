## Context

Builds on `UIEventRouter` (hit-test/focus/capture) and `UIElement` event hooks.

## Decisions

### 1. Multi-pointer capture
`UIPointerEvent` gains a `pointerId` (default 0). The router keeps a map of pointer id to captured element,
so simultaneous pointers capture independently. `GetCapture(id = 0)` preserves the single-pointer API.

### 2. Focus on press
On pointer down the router focuses the nearest focusable ancestor of the hit element (or clears focus when
none), so clicking a text field starts receiving key/text input.

### 3. Text input routing
Add `UIElement::OnTextInput`; `UIEventRouter::DispatchText` delivers a `UITextInputEvent` to the focused
element and bubbles while unhandled.

### 4. EditBox
`EditBox` is focusable and holds text plus a caret. It consumes pointer presses, handles backspace
(`0x08`) and left/right (`0x25`/`0x27`) keys, inserts committed text at the caret, and paints a theme
background plus the text through `UITextSystem` when one is set. Alternative: a full text layout engine —
out of scope.

## Risks / Trade-offs

- **Caret rendering/blink** -> this milestone draws no caret; only text content is painted.
- **Key codes are platform VK values** -> a key mapping layer is a follow-up.
- **Hover tracks pointer 0 only** -> multi-pointer hover is out of scope.
