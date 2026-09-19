## ADDED Requirements

### Requirement: Percent sizing
The layout system SHALL support a percent size mode where an axis resolves to a fraction of the parent
extent, and the document loader SHALL parse `"50%"`-style strings into that mode.

#### Scenario: Percent resolves against parent
- **WHEN** a child with a 50 percent width is laid out inside a 200-wide parent
- **THEN** the child width SHALL be 100

### Requirement: Z-ordered children
Elements SHALL carry a `z` value; painting SHALL draw children in ascending `z` with a stable order for equal
`z`, and hit-testing SHALL consider them in the reverse order.

#### Scenario: Higher z paints later
- **WHEN** two overlapping children differ in `z`
- **THEN** the higher-`z` child SHALL be painted after the lower one and SHALL be hit first

### Requirement: Linear layout containers
The UI system SHALL provide `HBox` and `VBox` containers that arrange their children sequentially along the
main axis with spacing, respecting container padding, and that size themselves to their content when AUTO.

#### Scenario: HBox places children in a row
- **WHEN** an HBox with spacing contains two fixed-size children
- **THEN** the second child's left SHALL equal the first child's right plus the spacing

#### Scenario: VBox content height
- **WHEN** a VBox has AUTO height and two stacked children
- **THEN** its measured height SHALL cover both children plus spacing and padding
