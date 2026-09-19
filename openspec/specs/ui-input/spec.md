# ui-input Specification

## Purpose
TBD - created by archiving change ui-input. Update Purpose after archive.
## Requirements
### Requirement: Multi-pointer capture
Pointer events SHALL carry a pointer id, and the router SHALL track capture per pointer so that each pointer's
move/release is delivered to the element it pressed, independently of other pointers.

#### Scenario: Independent captures
- **WHEN** two pointers press two different elements and then move
- **THEN** each element SHALL receive only its own pointer's events

#### Scenario: Release clears one capture
- **WHEN** a captured pointer is released
- **THEN** only that pointer's capture SHALL be cleared

### Requirement: Focus on press
A pointer press SHALL focus the nearest focusable ancestor of the hit element, so a text field begins
receiving input when clicked.

#### Scenario: Click focuses text field
- **WHEN** a press hits a focusable element
- **THEN** that element SHALL become focused and receive subsequent key and text events

### Requirement: Text input routing
Committed text input SHALL be delivered to the focused element and bubble to ancestors while unhandled.

#### Scenario: Text goes to focus
- **WHEN** text input is dispatched while an element is focused
- **THEN** the focused element SHALL receive an `OnTextInput` call

### Requirement: Edit box
An `EditBox` SHALL be a focusable text field that inserts committed text at its caret, deletes the character
before the caret on backspace, and moves the caret on left/right key input.

#### Scenario: Typing inserts text
- **WHEN** text is committed while an edit box is focused
- **THEN** the text SHALL be inserted at the caret and the caret SHALL advance

#### Scenario: Backspace deletes
- **WHEN** backspace is received
- **THEN** the character before the caret SHALL be removed

#### Scenario: Caret moves
- **WHEN** a left or right key is received
- **THEN** the caret SHALL move within the text bounds

