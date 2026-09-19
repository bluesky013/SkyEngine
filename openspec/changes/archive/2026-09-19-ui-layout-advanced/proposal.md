## Why

`ui-core` layout only supports point/stretch anchors with AUTO/FILL/FIXED sizing and paints children in
insertion order. Real screens need percentage sizing, explicit z-ordering, and linear (stacked) containers.

## What Changes

- Add `UISizeMode::PERCENT` (fraction of the parent extent) to the size rules and document `"50%"` parsing.
- Add a per-element `z` that orders children for painting and hit-testing (stable within equal z).
- Add `HBox`/`VBox` linear layout containers (spacing + padding) arranged through a new `ArrangeChildren` hook.
- Register `HBox`/`VBox` element types.

**Non-goals**: relative/percent-position layout managers, wrapping/grid layout, and scroll containers.

## Capabilities

### Modified Capabilities
- `ui-core`: add percent sizing, z-ordered children, and linear layout containers.

## Impact

- `engine/ui/core`: `UILayout.{h,cpp}`, `UIElement.{h,cpp}`, new `widgets/HBox.*`, `widgets/VBox.*`,
  registry, loader `z`/`"50%"` parsing; tests in `engine/test/ui`.
