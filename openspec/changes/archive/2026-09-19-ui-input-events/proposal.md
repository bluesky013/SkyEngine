## Why

`ui-core` input handles hit-testing, focus, and capture, but elements cannot observe hover, events do not
bubble when a child leaves them unhandled, and there is no keyboard focus traversal. These are core,
render-independent behaviors needed for interactive screens.

## What Changes

- Add pointer enter/leave events; `UIEventRouter` tracks the hovered element and notifies on change.
- Bubble unhandled events to ancestors (pointer and key), stopping at the first handling element.
- Add a focusable flag and `UIContext::FindNextFocus(current, forward)` for Tab-style traversal.

**Non-goals**: multi-touch, gestures, drag-and-drop, and a full routed event model with capture/tunnel phases.

## Capabilities

### Modified Capabilities
- `ui-core`: add hover enter/leave, unhandled-event bubbling, and focus traversal.

## Impact

- `UIElement` gains enter/leave hooks and a focusable flag; `UIEventRouter` gains hover + bubbling;
  `UIContext` gains focus traversal; tests in `engine/test/ui`.
