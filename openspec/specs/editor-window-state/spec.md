# editor-window-state Specification

## Purpose
TBD - created by archiving change editor-window-state. Update Purpose after archive.
## Requirements
### Requirement: Persist window geometry
The editor SHALL persist the main window's size and position per user and restore them on the next launch.

#### Scenario: Save on graceful exit
- **WHEN** the user closes the editor window normally
- **THEN** the current window client size and screen position SHALL be written to the user's editor state

#### Scenario: Restore on launch
- **WHEN** the editor starts and a valid saved window state exists
- **THEN** the main window SHALL open at the saved size and position

#### Scenario: No saved state
- **WHEN** no saved window state exists or it is malformed
- **THEN** the window SHALL open at the default size

#### Scenario: Bounded run does not overwrite
- **WHEN** the editor runs bounded (for example `--frames N`)
- **THEN** it SHALL NOT overwrite the saved window state

### Requirement: Live window geometry
The native window SHALL report its live client size and screen position so the state can be captured.

#### Scenario: Live size after resize
- **WHEN** the user resizes the window
- **THEN** the window size accessors SHALL report the new client size

#### Scenario: Position accessor
- **WHEN** the window has been moved
- **THEN** the window SHALL report its current screen position

