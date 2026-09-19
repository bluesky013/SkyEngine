## 1. Element hooks

- [x] 1.1 Add `OnPointerEnter`/`OnPointerLeave` virtuals and a `focusable` flag to `UIElement`
- [x] 1.2 Add `UIContext::FindNextFocus(current, forward)` with wrap-around

## 2. Router

- [x] 2.1 Track `hovered` in `UIEventRouter` and dispatch enter/leave on change
- [x] 2.2 Bubble unhandled pointer/key events to ancestors until handled

## 3. Tests

- [x] 3.1 Tests: enter/leave on move, child-unhandled bubbles to parent, focus traversal forward/back/wrap
- [x] 3.2 Build and run the suite green
