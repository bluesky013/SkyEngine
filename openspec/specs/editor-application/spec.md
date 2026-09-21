# editor-application Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
### Requirement: Native application shell without a UI toolkit
The editor application SHALL create its main window through the existing native window framework
(`Framework` `Platform` + `NativeWindow`, SDL-backed Win32/Cocoa) and SHALL own the event loop, without using a
UI toolkit for the application layer.

#### Scenario: Non-Qt window and event loop
- **WHEN** the editor application starts on Windows or macOS
- **THEN** it SHALL create a native window, expose its native handle, and pump events until exit, without Qt

### Requirement: Toolkit-independent module loading and frame driver
The application SHALL load modules from configuration and drive a per-frame tick that is independent of any UI
toolkit.

#### Scenario: Frame tick driven each loop
- **WHEN** the application main loop runs
- **THEN** it SHALL tick loaded modules each frame regardless of the UI framework

### Requirement: Shell services through the platform layer
File dialogs, clipboard, and text input/IME SHALL be provided by the native platform layer. The editor menu bar
and toolbars are engine-drawn and are specified by `editor-ui-shell`.

#### Scenario: File dialog returns a selection
- **WHEN** a panel requests a file through the platform layer
- **THEN** the platform SHALL present the OS file dialog and return the chosen path to the caller

#### Scenario: Text input enabled
- **WHEN** the shell starts and a text field is focused
- **THEN** text input/IME SHALL be provided by the platform layer

### Requirement: RHI-free shell prototype target
The opt-in `Sandbox` prototype (`engine/sandbox/src`) SHALL build with `SKY_BUILD_SANDBOX` and SHALL depend only
on `EditorCore` and `Framework`; it SHALL NOT link Aurora or the render stack. The core tests SHALL build with
`SKY_BUILD_TEST` and run headless.

#### Scenario: Sandbox builds and opens a window
- **WHEN** the project is configured with `SKY_BUILD_SANDBOX=ON`
- **THEN** the `Sandbox` target SHALL build, open a non-Qt native window, pump events, and exit cleanly with no
  Aurora/RHI dependency

#### Scenario: Headless tests run
- **WHEN** the project is configured with `SKY_BUILD_TEST=ON`
- **THEN** `EditorCoreTest` SHALL build and run without a window or GPU

