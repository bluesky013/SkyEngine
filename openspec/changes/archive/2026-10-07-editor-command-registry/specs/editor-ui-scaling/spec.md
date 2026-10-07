## ADDED Requirements

### Requirement: Single UI style source
The editor UI dimensions and typography SHALL come from a single theme (`UiTheme` with `UiMetrics` / `UiFonts`);
views SHALL derive sizes from the theme rather than hard-coding pixel layouts.

#### Scenario: Derived size
- **WHEN** a view needs a row height / padding / field height
- **THEN** it SHALL read it from the theme metrics (or an expression of them), not a literal

### Requirement: Uniform scaling
The theme SHALL provide a scale; applying it SHALL multiply every metric and font size together, so widget
sizes and text scale uniformly.

#### Scenario: Scale doubles
- **WHEN** the theme is built at 2× scale
- **THEN** every metric (e.g. row height) and font size SHALL be doubled

### Requirement: Chrome scales with DPI
The menu bar, toolbar, and status bar heights SHALL be theme metrics so the editor chrome scales with DPI like
the rest of the UI.

#### Scenario: High-DPI chrome
- **WHEN** the effective scale is above 1 (e.g. a 150% display)
- **THEN** the menu bar, toolbar, and status bar SHALL grow with the theme scale, not stay at fixed pixels

### Requirement: Effective scale from DPI and preference
The effective scale SHALL be the system DPI scale times a user preference (`editor.uiScale`), with an
environment override for development, and SHALL apply to the editor and the Project Manager hub.

#### Scenario: User preference multiplies DPI
- **WHEN** the display DPI scale is 1.5 and the user sets UI Scale to 1.25
- **THEN** the effective scale SHALL be 1.875

### Requirement: Reset Layout restores the built-out default
Resetting the layout SHALL restore the full default arrangement that was built (splits + panels), not a single
initial panel.

#### Scenario: Restore arrangement
- **WHEN** the default layout has been built and captured, the user rearranges/closes panels, and chooses
  Reset Layout
- **THEN** the full default arrangement SHALL be restored
