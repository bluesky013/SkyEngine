## ADDED Requirements

### Requirement: IConsoleUI abstract interface
The engine SHALL provide an `IConsoleUI` interface in `framework/console/IConsoleUI.h` with zero render dependencies. The interface SHALL define the contract for any in-game overlay console frontend.

#### Scenario: Interface has no render includes
- **WHEN** `IConsoleUI.h` is included by a module that does not link any render library
- **THEN** compilation SHALL succeed with no missing headers or symbols

### Requirement: Toggle visibility
`IConsoleUI` SHALL provide `Toggle()`, `Show()`, `Hide()`, and `IsVisible()` methods to control overlay visibility.

#### Scenario: Toggle from hidden
- **WHEN** the console is hidden and `Toggle()` is called
- **THEN** `IsVisible()` SHALL return `true`

#### Scenario: Toggle from visible
- **WHEN** the console is visible and `Toggle()` is called
- **THEN** `IsVisible()` SHALL return `false`

### Requirement: Input capture flag
`IConsoleUI` SHALL provide a `WantsInput()` method. When the console is visible, `WantsInput()` SHALL return `true`, indicating the input system SHALL suppress game-level keyboard input.

#### Scenario: Visible console claims input
- **WHEN** the overlay console is visible
- **THEN** `WantsInput()` SHALL return `true`

#### Scenario: Hidden console releases input
- **WHEN** the overlay console is hidden
- **THEN** `WantsInput()` SHALL return `false`

### Requirement: Push log entries
`IConsoleUI` SHALL provide `PushLog(const LogEntry &entry)` to receive log entries from the console system. The implementation SHALL store entries for display.

#### Scenario: Log entry pushed to overlay
- **WHEN** `PushLog()` is called with a LogEntry
- **THEN** the entry SHALL appear in the overlay's log scroll area on the next render frame

### Requirement: Poll submitted commands
`IConsoleUI` SHALL provide `PollCommand(std::string &outLine)` that returns `true` and fills `outLine` when the user has submitted a command via the overlay input field. Returns `false` when no command is pending.

#### Scenario: User submits command via overlay
- **WHEN** the user types "help" and presses Enter in the overlay input field
- **THEN** `PollCommand()` SHALL return `true` with `outLine == "help"`

#### Scenario: No command pending
- **WHEN** no command has been submitted
- **THEN** `PollCommand()` SHALL return `false`

### Requirement: Push command output
`IConsoleUI` SHALL provide `PushOutput(const std::string &text)` to display command execution results in the overlay. This is separate from log entries.

#### Scenario: Command result displayed
- **WHEN** `PushOutput("r.ShadowQuality = 3")` is called
- **THEN** the text SHALL appear in the overlay log area, visually distinct from log entries

### Requirement: Registration via Interface singleton
`IConsoleUI` implementations SHALL register themselves via `Interface<IConsoleUI>::Get()->Register(*this)` during module initialization and unregister during shutdown.

#### Scenario: ImGui implementation registered
- **WHEN** the ImGui render module initializes and registers an `IConsoleUI` implementation
- **THEN** `Interface<IConsoleUI>::Get()->GetApi()` SHALL return a non-null pointer

#### Scenario: No implementation registered
- **WHEN** no module registers an `IConsoleUI` implementation (e.g. headless mode)
- **THEN** `Interface<IConsoleUI>::Get()->GetApi()` SHALL return `nullptr`
