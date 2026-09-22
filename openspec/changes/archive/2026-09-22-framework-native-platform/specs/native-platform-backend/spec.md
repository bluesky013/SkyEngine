## ADDED Requirements

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

### Requirement: Native handle for the RHI
`NativeWindow::GetNativeHandle()` SHALL return the platform handle used by the RHI to create its surface
(`HWND` on Windows, a `CAMetalLayer`-backed view on macOS).

#### Scenario: Surface from handle
- **WHEN** the RHI creates a swapchain for a window
- **THEN** it SHALL use the handle from `GetNativeHandle()` unchanged

### Requirement: Clipboard and timing
The platform SHALL provide clipboard text access and high-resolution timing without SDL.

#### Scenario: Clipboard round-trip
- **WHEN** text is written to the clipboard and read back
- **THEN** the same text SHALL be returned

### Requirement: Text input and IME
The backend SHALL provide text input, and SHALL provide IME (composition) input for CJK in a later phase. Until
IME lands, plain text input SHALL work.

#### Scenario: Plain text input
- **WHEN** the user types ASCII text into a focused window
- **THEN** `OnTextInput` SHALL be broadcast with the entered text

#### Scenario: CJK composition (later phase)
- **WHEN** the user composes CJK text via the system IME
- **THEN** the composed text SHALL be delivered through `OnTextInput`
