## Why

There is no scrollable container or virtualized list, so any content larger than its view cannot be shown and
large lists cannot be bounded in memory. Both are render-independent.

## What Changes

- Add `ScrollView`: a clipping container with one content child, a clamped scroll offset, and wheel scrolling.
- Add `ListView`: a vertical virtualized list that only materializes items for the visible range.

**Non-goals**: horizontal-only lists, grids, page snapping, inertial scrolling, and scrollbars.

## Capabilities

### New Capabilities
- `ui-scroll`: scrollable containers and virtualized lists.

## Impact

- New `widgets/ScrollView.{h,cpp}` and `widgets/ListView.{h,cpp}`; tests in `engine/test/ui`.
