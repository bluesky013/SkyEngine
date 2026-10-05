## ADDED Requirements

### Requirement: Enum member widget

Enum members SHALL be rendered and edited by a registered widget like any other kind; the form view
SHALL NOT contain enum-specific drawing or popup logic.

#### Scenario: Enum edited through its widget

- **WHEN** a form shows an enum member
- **THEN** clicking the control SHALL open the enum options through the enum widget and selecting one
  SHALL set the value as one undoable edit

#### Scenario: View has no enum special case

- **WHEN** the form view renders or hit-tests a field
- **THEN** it SHALL delegate enum members to the widget registry rather than handling them inline
