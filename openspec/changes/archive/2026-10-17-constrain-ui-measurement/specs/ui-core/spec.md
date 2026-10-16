## MODIFIED Requirements

### Requirement: Layout system
The core SHALL provide measure and arrange stages supporting anchor and box-size rules, and SHALL expose each element's resolved bounds after layout. Its non-virtual MeasureDesired entry SHALL enforce normalized constraints through OnMeasure, and MeasureForLayout/Layout SHALL resolve known own-rule axes before measuring. Layout(parentRect) SHALL compute the requested rectangle from the same rule budget and enter Arrange(finalRect). A parent that already owns a child's final rectangle SHALL measure/arrange that allocation without applying the child's anchor/size rules again. Descendants SHALL be arranged before return, and absolute layout-surface coordinates SHALL remain unchanged. Legacy Measure(float&, float&) SHALL remain supported through the one-way bridge.

#### Scenario: Arrange computes bounds
- **WHEN** ordinary layout runs for an element with a fixed size inside a parent
- **THEN** its resolved bounds SHALL equal the requested fixed size, including an explicit size larger than the parent's available extent

#### Scenario: Layout respects parent shrink
- **WHEN** a child with a non-overflowing AUTO/FILL request is laid out inside a fixed parent content box
- **THEN** its resolved allocation SHALL respect that content budget

#### Scenario: Parent allocation is final
- **WHEN** a slot container allocates a child rectangle and calls the child's Arrange
- **THEN** the child's resolved bounds SHALL equal that rectangle without a second anchor or box-size resolution

#### Scenario: Nested container geometry exists before paint
- **WHEN** a container containing another slot container is arranged
- **THEN** the nested container and its descendants SHALL have resolved bounds before any paint call

#### Scenario: Parent resize updates descendants
- **WHEN** an arranged container is arranged again with a different final rectangle
- **THEN** its descendant slot allocations SHALL reflect the new rectangle before paint

#### Scenario: Known width constrains desired height
- **WHEN** an element's width is resolved before measuring width-dependent content
- **THEN** OnMeasure SHALL receive that resolved width budget before producing desired height

### Requirement: Linear layout containers
The UI system SHALL provide HBox and VBox containers that arrange their children sequentially along the main axis with spacing and padding and size themselves to their content when AUTO. They SHALL query child layout rules with available constraints, reserve consumed Fixed/Auto extents and spacing, and allocate Fill only from the nonnegative remaining main-axis budget. Their final child allocations SHALL NOT overlap because a Fill child receives the original full parent extent again.

#### Scenario: HBox places children in a row
- **WHEN** an HBox with spacing contains two fixed-size children
- **THEN** the second child's left SHALL equal the first child's right plus the spacing

#### Scenario: VBox content height
- **WHEN** a VBox has AUTO height and two stacked children
- **THEN** its measured height SHALL cover both children plus spacing and padding, subject to the caller's hard constraints

#### Scenario: Fill follows an already allocated sibling
- **WHEN** a finite HBox or VBox has a non-Fill child followed by Fill content
- **THEN** the Fill child's allocated extent SHALL be the remaining main-axis share rather than the full parent extent
