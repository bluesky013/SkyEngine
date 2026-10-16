# ui-scroll Specification

## Purpose
TBD - created by archiving change ui-scroll. Update Purpose after archive.
## Requirements
### Requirement: Scrollable container
ScrollView SHALL clip its content and expose ScrollAxes Both, Horizontal, Vertical, and None, with Both as the default. Content measurement SHALL permit full unbounded AUTO extent on scrolling axes and SHALL hard-bound non-scrolling axes to the viewport. An AUTO viewport query SHALL first measure under the available budget and clamp a candidate outer viewport. Vertical, Both, and None SHALL retain its candidate width and remeasure dependent height at the resulting actual inner width; Horizontal SHALL retain its candidate height and remeasure width at the resulting actual inner height, avoiding repeated Percent-driven shrinking of the frozen axis. Content Fixed/Percent/Fill rules SHALL resolve once against the viewport reference for each query; the final content rectangle SHALL then be assigned through Arrange without a second Layout resolution. Stored content extent SHALL equal the final allocated content size, and enabled scroll offsets SHALL be clamped to max(0, contentExtent - viewportExtent). Disabled axes SHALL NOT retain scroll offset. Wheel input SHALL change vertical offset when vertical scrolling is enabled. Measurement SHALL NOT change offsets, arranged extent, bounds, or tree state. Removing/clearing content through inherited child operations SHALL clear the cached content pointer.

#### Scenario: Scroll clamps to content
- **WHEN** the scroll offset is set beyond the content bounds and the view is arranged
- **THEN** it SHALL be clamped using the same final content extent as the arranged content rectangle

#### Scenario: Wheel scrolls
- **WHEN** a wheel event is delivered to a view with vertical scrolling enabled
- **THEN** the vertical offset SHALL change by the wheel delta and SHALL clamp during arrangement

#### Scenario: Content is clipped
- **WHEN** content is larger than the view
- **THEN** drawn content SHALL be clipped to the view bounds

#### Scenario: Vertical scrolling constrains wrapping width
- **WHEN** a Vertical view measures AUTO wrapping content
- **THEN** the width SHALL be bounded by the viewport and the height SHALL be measured as a full unbounded scroll extent

#### Scenario: Non-scroll axis is a hard viewport maximum
- **WHEN** a child declares Fixed width 500 in a Vertical view of width 200
- **THEN** its final measured/arranged width SHALL be no greater than 200 while height can extend on the scrolling axis

#### Scenario: AUTO viewport retains its allocated wrapping width
- **WHEN** a Vertical, Both, or None view chooses an AUTO viewport width from content with a Percent width rule
- **THEN** it SHALL freeze the first clamped candidate width and measure dependent height at that actual inner width rather than repeatedly shrinking the viewport with the Percent rule

#### Scenario: Horizontal AUTO viewport retains its allocated height
- **WHEN** a Horizontal view chooses an AUTO viewport height
- **THEN** it SHALL freeze the first clamped candidate height and remeasure width at that actual inner height

#### Scenario: Content rules use the viewport once
- **WHEN** content declares Percent or Fill size
- **THEN** that rule SHALL use the viewport as its reference once and the resulting final rectangle SHALL NOT resolve it a second time

#### Scenario: ScrollView owns final content placement
- **WHEN** content declares anchors or offsets and its extent has been resolved against the viewport
- **THEN** its final origin SHALL be view.left minus scrollX and view.top minus scrollY, without reapplying content positioning offsets

#### Scenario: Both and None have explicit policies
- **WHEN** Both or None is selected
- **THEN** Both SHALL allow full AUTO extent on both scrolling axes, while None SHALL hard-bound both axes to the viewport and disable both offsets

#### Scenario: Removed content is safe
- **WHEN** the content child is removed or children are cleared through inherited operations
- **THEN** the next measure/arrange SHALL NOT access a stale cached content pointer

### Requirement: Virtualized list
ListView SHALL show fixed-height items and materialize only the visible range for a given arranged viewport/scroll offset. Measurement SHALL use the full item-count extent independently of currently live rows, subject to caller constraints, and SHALL NOT invoke the factory, change live items, or alter scroll state. Arrangement SHALL create/remove visible items and assign their final item rectangles through Arrange without reapplying child layout rules.

#### Scenario: Only visible items exist
- **WHEN** a list with many items is laid out in a short view
- **THEN** the number of live item elements SHALL correspond to the visible range rather than the item count

#### Scenario: Scrolling changes the visible range
- **WHEN** the list is scrolled down by several item heights and arranged
- **THEN** materialized indices SHALL shift to cover the newly visible range

#### Scenario: Measurement creates no rows
- **WHEN** a list with an item factory is measured before first arrangement
- **THEN** measurement SHALL report its fixed-item extent under the budget without invoking the factory or creating children

#### Scenario: Visible rows respect final slots
- **WHEN** a visible item declares conflicting own layout rules
- **THEN** its arranged bounds SHALL still equal the item rectangle allocated by the list
