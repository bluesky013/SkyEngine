## ADDED Requirements

### Requirement: Theme-driven panel styling

The editor shell SHALL style its surfaces through the UI theme (`UITheme`/`UIStyle`), so panel, title, menu and
button colors come from a single theme rather than hardcoded per-panel values.

#### Scenario: Panels resolve colors from the theme
- **WHEN** a panel is painted
- **THEN** its background and title colors SHALL come from the theme's resolved style for the panel's classes

#### Scenario: Changing the theme changes the shell
- **WHEN** a theme style value is changed
- **THEN** the affected shell surfaces SHALL render with the new value, with no per-panel constant to change

### Requirement: Tab headers switch the active panel

A tab that contains more than one panel SHALL render a header row of its panel titles, and selecting a header SHALL
make that panel the tab's active panel and show its view.

#### Scenario: Switch tabs
- **WHEN** the user selects a panel title in a multi-panel tab's header
- **THEN** that panel SHALL become active and its view SHALL be shown

#### Scenario: Only the active panel body is shown
- **WHEN** a tab has multiple panels
- **THEN** only the active panel's body SHALL be laid out and painted

### Requirement: Engine-drawn tool/menu bar

The shell SHALL provide an engine-drawn tool/menu bar with labeled action items; selecting an item SHALL run the
associated editor action. Items SHALL be grouped (for example a View group whose items toggle panel visibility).

#### Scenario: Run a toolbar item
- **WHEN** the user selects an item in the tool/menu bar
- **THEN** the associated editor action SHALL run

#### Scenario: View group toggles a panel
- **WHEN** the user selects a panel item in the View group
- **THEN** that panel SHALL be hidden if shown, or shown if hidden
