## MODIFIED Requirements

### Requirement: Event loop and event mapping
The backend SHALL own the message/event loop and SHALL map native events to the engine events: window
resize/focus/close to `IWindowEvent`, and key down/up and text to `IKeyboardEvent`.

#### Scenario: Resize event
- **WHEN** a window is resized
- **THEN** an `IWindowEvent::OnWindowResize` SHALL be broadcast with the new size and window id

#### Scenario: Key and text events
- **WHEN** the user presses a key or enters text
- **THEN** `IKeyboardEvent::OnKeyDown`/`OnKeyUp` and `OnTextInput` SHALL be broadcast

#### Scenario: Close event before destruction
- **WHEN** a window receives a close request
- **THEN** an `IWindowEvent::OnWindowClose` SHALL be broadcast for that window before it is destroyed

#### Scenario: Closing the main window exits
- **WHEN** the main window receives a close request
- **THEN** the backend SHALL post a quit message so the message loop exits

#### Scenario: Closing a secondary window does not exit
- **WHEN** a secondary window (not the main window) receives a close request
- **THEN** only that window SHALL be destroyed and the application SHALL keep running
