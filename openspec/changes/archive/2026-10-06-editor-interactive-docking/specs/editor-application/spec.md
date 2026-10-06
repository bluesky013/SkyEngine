## MODIFIED Requirements

### Requirement: Shell services through the platform layer
File dialogs, clipboard, and text input/IME SHALL be provided by the native platform layer. The editor menu bar
is engine-drawn and is specified by `editor-ui-shell`.

#### Scenario: File dialog returns a selection
- **WHEN** a panel requests a file through the platform layer
- **THEN** the platform SHALL present the OS file dialog and return the chosen path to the caller

#### Scenario: Text input enabled
- **WHEN** the shell starts and a text field is focused
- **THEN** text input/IME SHALL be provided by the platform layer
