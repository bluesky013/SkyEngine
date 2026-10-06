# editor-application Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
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

### Requirement: Toolkit-independent module loading and frame driver
The application SHALL load modules from configuration and drive a per-frame tick that is independent of any UI
toolkit. When a quit has been requested (for example the main window closed), the loop SHALL stop before ticking
modules so no frame is rendered against a window that is being destroyed.

#### Scenario: Frame tick driven each loop
- **WHEN** the application main loop runs
- **THEN** it SHALL tick loaded modules each frame regardless of the UI framework

#### Scenario: Quit stops the frame before ticking
- **WHEN** a quit is requested during event pumping (for example the main window closed)
- **THEN** the loop SHALL skip the module tick/rendering for that frame and exit

### Requirement: Shell services through the platform layer
File dialogs, clipboard, and text input/IME SHALL be provided by the native platform layer. The editor menu bar
is engine-drawn and is specified by `editor-ui-shell`.

#### Scenario: File dialog returns a selection
- **WHEN** a panel requests a file through the platform layer
- **THEN** the platform SHALL present the OS file dialog and return the chosen path to the caller

#### Scenario: Text input enabled
- **WHEN** the shell starts and a text field is focused
- **THEN** text input/IME SHALL be provided by the platform layer

### Requirement: Editor host target

The editor host SHALL be startable either by the dedicated `SandboxEditor` executable (`engine/sandbox/app`) or by
the launcher in editor mode (`--app editor`); both SHALL run the same editor host logic. The editor host SHALL load
its modules from `configs/modules_editor.json` (falling back to `SandboxModule` when the config is absent). The
`SandboxEditor` target and the editor mode SHALL build with `SKY_BUILD_SANDBOX`; the core tests SHALL build with
`SKY_BUILD_TEST` and run headless.

#### Scenario: Editor host builds and runs the module
- **WHEN** the project is configured with `SKY_BUILD_SANDBOX=ON`
- **THEN** the `SandboxEditor` target SHALL build, open a non-Qt native window, and run the editor module

#### Scenario: Launcher editor mode runs the editor
- **WHEN** the launcher is started with `--app editor` and the sandbox editor is built
- **THEN** it SHALL run the editor host with modules from `configs/modules_editor.json`

#### Scenario: Headless tests run
- **WHEN** the project is configured with `SKY_BUILD_TEST=ON`
- **THEN** `EditorCoreTest` SHALL build and run without a window or GPU

