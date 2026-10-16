## Why

The UI now has explicit arrangement and consistent render/input geometry, but intrinsic-only measurement cannot tell wrapping content or nested containers how much width they actually receive. Scroll extents and viewer content heights can consequently be computed from a different size than the final allocation. This change establishes a bounded measurement query and carries its budget through containers, scrolling, text, and an editor consumer.

## What Changes

- Add pure UISize/UIConstraints normalization, deflation, finite-result clamping, and ResolveMeasureConstraints utilities to engine/ui.
- Add non-virtual MeasureDesired(constraints), protected OnMeasure, and MeasureForLayout compatibility paths. Preserve legacy Measure overrides through a one-way adapter; normalize and enforce hard constraints at the common entry.
- Resolve known Fixed/Fill/Percent/anchor axes before measuring, then arrange from the same rule budget. Ordinary explicit Fixed sizes may exceed parent available size; a parent-owned tight slot remains authoritative.
- Carry constraints through HBox/VBox/StackPanel/DockPanel/GridPanel/Overlay, allocating remaining Fill space without overlapping preceding children or reapplying child rules to final slot rectangles.
- Add ScrollAxes Both/Horizontal/Vertical/None with Both as the compatibility default. Measure full extent on scroll axes and enforce viewport bounds on other axes; resolve content rules once against the viewport and align final extent/clamping with the arranged rectangle.
- Keep ListView measurement free of item creation and preserve full fixed-item extent, materializing/arranging only the visible range.
- Add default-off byte-wise soft wrap to Text, sharing line layout between measurement and paint, honoring padding/per-line alignment, and exposing boolean text.wrap through generic property/JSON construction.
- Measure AssetViewer custom content with its actual column width after scrollbar reservation.
- Measurement queries do not alter bounds, scroll offsets, the tree, or dirty state. Geometry, cache/invalidation, shaping, and resource/surface ownership redesign stay outside this change.

## Capabilities

### New Capabilities

- `ui-measurement`: pure constraint data, finite query semantics, legacy adaptation, size-rule budgets, container propagation, and width-aware host measurement.

### Modified Capabilities

- `ui-core`: constraint-aware layout integration and linear remaining-space allocation while preserving parent-owned final arrangement.
- `ui-scroll`: explicit axis policy, consistent content extent/clamping, content-child removal safety, and side-effect-free virtual-list measurement.
- `ui-text`: optional soft wrapping with shared measure/paint line layout and padding/alignment rules.
- `ui-data`: declarative text.wrap property parity with C++ construction.

## Impact

- engine/ui pure size/constraint/layout utilities; engine/ui/core runtime element/container/scroll/list/text implementation and tests.
- AssetViewerWidget's custom-content measurement and editor layout documentation.
- Builds on the existing uncommitted UI foundation, shared editor widgets, and geometry changes without modifying or archiving those change artifacts or main specs.
- Existing documents default to no wrap, legacy Measure callers remain supported, and absolute geometry/input coordinates and renderer contracts do not change.
