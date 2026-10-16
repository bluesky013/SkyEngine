## Why

UI transforms currently move painted vertices without consistently moving hit-tests, clipping, or the coordinates delivered to widget handlers. Culling also tests untransformed quads, and rounded-shape shaders compare framebuffer coordinates against layout-space shape data. One geometry contract is needed so existing absolute-layout widgets remain interactive and visually correct under nested and surface transforms.

## What Changes

- Add pure `UIGeometry`, affine inversion, and transformed-rectangle AABB utilities to `engine/ui`/`UI.Interface`; keep tree traversal, painting, and input implementation in `engine/ui/core`.
- Preserve `GetBounds` and `Arrange(finalRect)` as absolute layout-surface rectangles. Add `UIElement::GetGeometry(extraTransform = {})` for bounds plus the cumulative layout-to-rendered-surface transform, including the context surface base by default and optionally premultiplying an extra compositor transform, computed on demand without a geometry cache.
- Add persistent UIContext surface-transform configuration available before first paint. Keep the explicit `Paint(context, rootTransform)` overload as a compatibility entry that stores the transform before painting; paint and input then share it.
- Make clip pushes transform their layout rectangles into rendered-surface AABBs, intersect them with the unchanged root surface clip, and cull quads after transforming them.
- Route hit-tests and pointer coordinates through inverse geometry for each target, ancestor, hovered element, and captured pointer. Keep handler `x/y` in that receiver's untransformed absolute layout coordinates; direct child routing consumes parent-layout coordinates.
- Add a coordinate-free pointer cancellation notification when captured geometry becomes non-invertible, clearing router/direct capture and dependent pressed/drag/preview state rather than leaving a gesture stuck after its release is rejected.
- Keep rotated clipping conservative and axis-aligned for the existing scissor renderer. Evaluate rounded-shape SDFs in interpolated layout coordinates with a shared renderer shader source that can also be compiled offline.
- Leave constrained Measure, ScrollView content-extent behavior, invalidation/cache optimization, RDG integration, and host surface ownership to later changes.

## Capabilities

### New Capabilities

- `ui-geometry`: pure geometry/inverse/AABB data contract, on-demand accumulated geometry, persistent context surface transform, and transformed paint clipping/culling.

### Modified Capabilities

- `ui-core`: transform, draw-data, and hit-test behavior use the same accumulated geometry while retaining absolute layout bounds.
- `ui-input`: inverse-mapped per-receiver pointer coordinates, transformed capture/hover/bubbling, and consistent direct child routing.
- `ui-render`: rounded-shape shader calculations use layout-space coordinates and share their actual source with offline compilation validation.

## Impact

- `engine/ui` pure geometry types/utilities; `engine/ui/core` element/context/paint/router/child-routing implementations; Aurora UI renderer shape shader and its validation.
- Focused geometry, input, paint, and rounded-shape regressions; editor integration builds retain existing layout and event-handler coordinates when transforms are identity.
- Builds on the uncommitted `refactor-ui-foundations` and `extract-editor-widgets` source changes. Their files and main specs remain intact; this work does not commit or archive any change.
- No persisted document format, Measure API, DPI scaling rule, resource ownership model, or renderer recording model change.
