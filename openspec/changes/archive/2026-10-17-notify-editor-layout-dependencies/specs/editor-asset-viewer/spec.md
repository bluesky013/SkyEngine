## ADDED Requirements

### Requirement: Asset viewer notifies incremental composition changes
The viewer SHALL synchronize its owned reflected forms before calculating stacked content extent, scrollbar reservation, scroll clamp, and final child rectangles. Its audited owned composition SHALL support incremental arrangement with notified open/reload/rebuilt-content, panel-size, and scroll changes. Reopening at unchanged outer bounds SHALL still allocate the new content before paint. Unknown provider content SHALL retain its own runtime volatility policy and SHALL NOT be forced stable by the viewer. Custom content SHALL continue to be measured and arranged at its actual content-column width.

#### Scenario: New forms have correct first-frame extent
- **WHEN** an asset opens with reflected target/data forms and the viewer arranges before its first Paint
- **THEN** its content total, scrollbar, and allocated form heights SHALL use the current form extents

#### Scenario: Viewer repeats an unchanged controlled frame
- **WHEN** an open controlled viewer completes layout and no input or volatile descendant changes
- **THEN** its next same-rectangle incremental allocation SHALL be eligible to skip completed arrangement

#### Scenario: Same-bounds reopen or reload replaces content
- **WHEN** the viewer reopens or reloads content while its outer bounds stay unchanged
- **THEN** the notified layout work SHALL update its forms, extent, scroll clamp, and child allocations before paint

#### Scenario: Standalone caller forces arrangement after a notification
- **WHEN** an existing standalone caller uses forced Arrange followed by Paint after an owned form's dependency change
- **THEN** the executing viewer arrangement SHALL synchronize its forms before computing their content extent without requiring a preliminary Paint

#### Scenario: Resizing or scrolling updates geometry
- **WHEN** panel size or outer content scroll changes
- **THEN** the next incremental allocation SHALL update content widths/origins and scroll clamping while preserving reusable measurements for a scroll-only change where valid

#### Scenario: Provider remains externally dynamic
- **WHEN** custom provider content remains default-volatile and changes its desired extent externally
- **THEN** the viewer's incremental allocation SHALL continue to observe the provider's current extent and arrange it at the actual reserved content width

#### Scenario: Theme changes copied chrome metrics
- **WHEN** the default theme changes toolbar or menu bar height after the viewer has been constructed
- **THEN** viewer synchronization SHALL update those owned chrome inputs before allocating the toolbar and body rather than retaining constructor-copied heights
