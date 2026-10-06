# native-platform-backend Specification

## Purpose
TBD - created by archiving change framework-native-platform. Update Purpose after archive.
## Requirements
### Requirement: Native window backend behind unchanged interfaces
The window/event layer SHALL be implemented with native OS APIs behind the unchanged `Platform` and
`NativeWindow` interfaces: Win32 on Windows and Cocoa on macOS. It SHALL NOT depend on SDL. Code above the seam
(launcher, editor host, runtime) SHALL NOT require changes.

#### Scenario: Build and run without SDL
- **WHEN** the project is built for Windows or macOS
- **THEN** the window/event backend SHALL use Win32/Cocoa and SHALL NOT link SDL

#### Scenario: Interfaces unchanged
- **WHEN** an existing host (launcher, editor host) is built against the native backend
- **THEN** it SHALL compile and behave the same without source changes

### Requirement: Multiple windows
The backend SHALL support creating more than one window in a process, each with its own native handle and event
stream.

#### Scenario: Second window
- **WHEN** a second `NativeWindow` is created
- **THEN** it SHALL be created successfully and its events SHALL be dispatched to the correct window

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

### Requirement: Native handle for the RHI
`NativeWindow::GetNativeHandle()` SHALL return the platform handle used by the RHI to create its surface
(`HWND` on Windows, the window's `CAMetalLayer` on macOS — the Metal swapchain and the MoltenVK
`VK_EXT_metal_surface` path both consume the layer directly).

#### Scenario: Surface from handle
- **WHEN** the RHI creates a swapchain for a window
- **THEN** it SHALL use the handle from `GetNativeHandle()` unchanged

#### Scenario: macOS layer handle
- **WHEN** a `NativeWindow` is created on macOS
- **THEN** its content view SHALL be backed by a `CAMetalLayer` and `GetNativeHandle()` SHALL return that layer

### Requirement: Clipboard and timing
The platform SHALL provide clipboard text access and high-resolution timing without SDL.

#### Scenario: Clipboard round-trip
- **WHEN** text is written to the clipboard and read back
- **THEN** the same text SHALL be returned

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


### Requirement: Window DPI scale
`NativeWindow::GetDpiScale()` SHALL return the window's device-pixels-per-point ratio and SHALL reflect
the display the window currently occupies (Windows: per-monitor DPI; macOS: the window's
`backingScaleFactor`), so consumers such as the sandbox editor viewport receive the correct scale.

#### Scenario: Retina window reports 2.0
- **WHEN** a `NativeWindow` is created on a 2x display on macOS
- **THEN** `GetDpiScale()` SHALL return `2.0f` (not the `1.0f` default)

#### Scenario: Scale follows the display
- **WHEN** the window moves to a display with a different backing scale
- **THEN** `GetDpiScale()` SHALL subsequently report the new scale without recreating the window
