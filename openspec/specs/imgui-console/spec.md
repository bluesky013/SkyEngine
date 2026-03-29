## ADDED Requirements

### Requirement: ImGui overlay console widget
The engine SHALL provide `ImGuiConsoleWidget` as an `ImWidget` in `render/imgui/` that implements `IConsoleUI`. It SHALL render a UE-style half-screen dropdown overlay using ImGui.

#### Scenario: Widget renders when visible
- **WHEN** the console is visible and `Execute(ImContext&)` is called
- **THEN** the widget SHALL draw a semi-transparent window covering the top portion of the screen with a log scroll area and an input text field

#### Scenario: Widget skips rendering when hidden
- **WHEN** the console is hidden and `Execute(ImContext&)` is called
- **THEN** the widget SHALL not draw anything

### Requirement: Tilde key toggles console
The ImGui console widget SHALL toggle visibility when the grave accent / tilde (`~` / backtick) key is pressed. The key SHALL be consumed and not passed to other input handlers when toggling.

#### Scenario: Press tilde to open
- **WHEN** the console is hidden and the user presses the `~` key
- **THEN** the console SHALL become visible and the key event SHALL be consumed

#### Scenario: Press tilde to close
- **WHEN** the console is visible and the user presses the `~` key
- **THEN** the console SHALL become hidden and the key event SHALL be consumed

### Requirement: Log display area
The console overlay SHALL display log entries in a scrollable area. Each entry SHALL be color-coded by level: red for ERROR, yellow for WARNING, default for INFO/DEBUG.

#### Scenario: Color-coded log display
- **WHEN** an ERROR log entry is pushed
- **THEN** it SHALL be displayed in red text in the log scroll area

#### Scenario: Auto-scroll to newest
- **WHEN** a new log entry is pushed and the user has not manually scrolled up
- **THEN** the log area SHALL automatically scroll to show the newest entry

#### Scenario: Manual scroll preserved
- **WHEN** the user has scrolled up in the log area and a new entry arrives
- **THEN** the scroll position SHALL remain where the user left it

### Requirement: Command input field
The console overlay SHALL provide a text input field at the bottom. Pressing Enter SHALL submit the text as a command and clear the field.

#### Scenario: Submit command
- **WHEN** the user types "set r.ShadowQuality 3" and presses Enter
- **THEN** the text SHALL be submitted via `PollCommand()`, the input field SHALL be cleared, and keyboard focus SHALL remain on the input field

#### Scenario: Input field focused when visible
- **WHEN** the console becomes visible
- **THEN** the input text field SHALL receive keyboard focus automatically

### Requirement: Command history navigation
The input field SHALL support Up/Down arrow keys to navigate command history (same CommandHistory instance used by the terminal).

#### Scenario: Up arrow recalls previous command
- **WHEN** the user presses Up arrow in the input field
- **THEN** the input field SHALL display the previous command from history

#### Scenario: Down arrow navigates forward
- **WHEN** the user has navigated back in history and presses Down arrow
- **THEN** the input field SHALL display the next command in history

### Requirement: Tab completion in overlay
The input field SHALL support Tab key for command/CVar name completion using `CommandRegistry::FindByPrefix()`.

#### Scenario: Single match auto-completes
- **WHEN** the user types "sys.ex" and presses Tab
- **THEN** the input field SHALL be filled with "sys.exit"

#### Scenario: Multiple matches shown
- **WHEN** the user types "r." and presses Tab and multiple matches exist
- **THEN** a list of matching commands/CVars SHALL be displayed below the input field

### Requirement: Widget lifecycle
`ImGuiConsoleWidget` SHALL register itself with `Interface<IConsoleUI>` during construction and be added to the active `ImGuiInstance` via `AddWidget()`. It SHALL unregister and remove itself on destruction.

#### Scenario: Registration on creation
- **WHEN** `ImGuiConsoleWidget` is constructed
- **THEN** `Interface<IConsoleUI>::Get()->GetApi()` SHALL return a pointer to this widget

#### Scenario: Cleanup on destruction
- **WHEN** `ImGuiConsoleWidget` is destroyed
- **THEN** `Interface<IConsoleUI>::Get()->GetApi()` SHALL return `nullptr`
