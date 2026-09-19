## ADDED Requirements

### Requirement: Scrollable container
A `ScrollView` SHALL clip its content and expose a scroll offset that is clamped to the content size minus the
view size, and SHALL change the vertical offset in response to wheel input.

#### Scenario: Scroll clamps to content
- **WHEN** the scroll offset is set beyond the content bounds
- **THEN** it SHALL be clamped so the content edge does not pass the view edge

#### Scenario: Wheel scrolls
- **WHEN** a wheel event is delivered to the scroll view
- **THEN** the vertical offset SHALL change by the wheel delta, clamped

#### Scenario: Content is clipped
- **WHEN** content is larger than the view
- **THEN** drawn content SHALL be clipped to the view bounds

### Requirement: Virtualized list
A `ListView` SHALL show items of a fixed height and materialize only the items in the visible range for a
given scroll offset.

#### Scenario: Only visible items exist
- **WHEN** a list with many items is laid out in a short view
- **THEN** the number of live item elements SHALL correspond to the visible range, not the item count

#### Scenario: Scrolling changes the visible range
- **WHEN** the list is scrolled down by several item heights
- **THEN** the materialized indices SHALL shift to cover the newly visible range
