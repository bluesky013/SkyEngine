## ADDED Requirements

### Requirement: Panel visibility through the layout model

The editor SHALL express panel visibility through the layout model: hiding a panel uses the model's close
operation, and showing a hidden panel re-adds it to the layout; the shell SHALL NOT track hidden panels itself.

#### Scenario: Hide a panel
- **WHEN** a panel is hidden (e.g. from the View menu)
- **THEN** it SHALL be removed from the layout model and the shell SHALL rebuild without it

#### Scenario: Show a hidden panel
- **WHEN** a previously hidden panel is shown
- **THEN** it SHALL be re-added to the layout model (split/tab) and the shell SHALL rebuild with it
