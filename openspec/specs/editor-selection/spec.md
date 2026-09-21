# editor-selection Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
### Requirement: Observable editor selection
The editor SHALL expose the current selection (world/entities/assets) and the active editor context as a service
independent of any UI toolkit, allowing views to subscribe and be notified of changes.

#### Scenario: Subscribers notified on change
- **WHEN** the selection changes
- **THEN** all subscribed views SHALL be notified of the new selection

#### Scenario: Selection is read without a toolkit
- **WHEN** the selection is queried in a headless test
- **THEN** it SHALL return the current selection without requiring a window or UI toolkit

### Requirement: Single owner of selection
The selection SHALL be owned by the service; the outliner, inspector, and viewport SHALL observe it rather than
holding independent copies.

#### Scenario: Two views stay consistent
- **WHEN** the outliner changes the selection
- **THEN** the inspector and viewport SHALL observe the same updated selection

