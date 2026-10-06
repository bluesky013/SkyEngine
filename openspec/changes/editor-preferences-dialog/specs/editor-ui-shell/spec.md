## ADDED Requirements

### Requirement: Preferences entry in the File menu
The File menu SHALL include a `Preferences…` command that opens the Preferences dialog, and the docked
`Config` panel SHALL no longer be part of the default layout or the View menu.

#### Scenario: Preferences command
- **WHEN** the user opens the File menu and selects `Preferences…`
- **THEN** the Preferences dialog SHALL open (see `editor-preferences`)

#### Scenario: Config panel retired
- **WHEN** the editor starts with the default layout
- **THEN** there SHALL be no `Config` panel/tab, and no `Config` entry in the View menu
