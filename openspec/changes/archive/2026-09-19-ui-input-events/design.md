## Context

Builds on `UIEventRouter` (hit-test/focus/capture) and `UIElement` event hooks.

## Decisions

### 1. Hover enter/leave
The router keeps the current `hovered` element. On a non-captured pointer move it re-hit-tests; when the target
changes it calls `OnPointerLeave` on the old and `OnPointerEnter` on the new. Enter/leave are notifications and
do not consume the move.

### 2. Event bubbling
Unhandled events bubble to ancestors: dispatch walks target -> parent -> ... calling the handler until one
returns `HANDLED` or the root is passed. Pointer down/up refuse to initiate on a miss (return unhandled).
Alternative: tunneling/capture phases — deferred (documented non-goal).

### 3. Focus traversal
Elements gain `focusable` (default false). `UIContext::FindNextFocus` collects enabled, effectively-visible,
focusable elements in traversal order and returns the next/previous with wrap-around; null current starts at
the ends.

## Risks / Trade-offs

- **Hover is not cleared when an element is removed or hidden** -> the next move re-hit-tests and clears it.
- **Bubbling ignores clipping of ancestors** -> only hit-testing is clip-aware; acceptable for notifications.
