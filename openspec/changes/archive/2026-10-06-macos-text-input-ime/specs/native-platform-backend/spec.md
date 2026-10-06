# Delta: native-platform-backend

## MODIFIED Requirements

### Requirement: Text input and IME
The backend SHALL provide text input. macOS SHALL provide IME (composition) input for CJK by implementing
`NSTextInputClient`; committed text (including an IME commit) SHALL be delivered through `OnTextInput`, while
in-flight composition (marked text) SHALL NOT be broadcast (the event interface has only `OnTextInput`). Any
in-flight composition SHALL be discarded when the window loses focus. Windows IME SHALL be provided in a later
phase; until then, plain text input SHALL work on Windows.

#### Scenario: Plain text input
- **WHEN** the user types ASCII text into a focused window
- **THEN** `OnTextInput` SHALL be broadcast with the entered text

#### Scenario: Command chords do not produce text
- **WHEN** the user presses a Command-modified key (e.g. Cmd+C)
- **THEN** no `OnTextInput` SHALL be broadcast for it

#### Scenario: CJK composition (macOS)
- **WHEN** the user composes CJK text via the system IME on macOS
- **THEN** the committed text SHALL be delivered through `OnTextInput`, and the composition SHALL be dropped if the window loses focus before commit
