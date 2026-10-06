## MODIFIED Requirements

### Requirement: Native application shell without a UI toolkit
The editor application SHALL create its main window through the existing native window framework
(`Framework` `Platform` + `NativeWindow`, SDL-backed Win32/Cocoa) and SHALL own the event loop, without using a
UI toolkit for the application layer. The main window SHALL be created with the persisted size and position
when a valid saved window state exists, and its geometry SHALL be saved on graceful exit.

#### Scenario: Non-Qt window and event loop
- **WHEN** the editor application starts on Windows or macOS
- **THEN** it SHALL create a native window, expose its native handle, and pump events until exit, without Qt

#### Scenario: Window opens with restored geometry
- **WHEN** the editor starts and a saved window state exists
- **THEN** the created window SHALL use the saved size and position

#### Scenario: Geometry saved on exit
- **WHEN** the editor window is closed normally
- **THEN** the window geometry SHALL be written to the user's editor state before exit
