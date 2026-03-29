## ADDED Requirements

### Requirement: Non-blocking terminal input
The system SHALL provide an `ITerminalIO` interface with a `PollChar(char&)` method that returns immediately whether or not input is available. It SHALL NOT block the calling thread.

#### Scenario: No input pending
- **WHEN** `PollChar()` is called and no key has been pressed
- **THEN** it SHALL return `false` immediately without blocking

#### Scenario: Input available
- **WHEN** the user presses a key and `PollChar()` is called
- **THEN** it SHALL return `true` and write the character to the output parameter

### Requirement: Win32 terminal backend
On Windows (`SKY_PLATFORM_WINDOWS`), the system SHALL provide a `Win32TerminalIO` implementation that uses `PeekConsoleInput()` / `ReadConsoleInput()` for non-blocking input and `WriteConsoleW()` or VT sequence output via `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)`.

#### Scenario: VT mode enabled on Windows 10+
- **WHEN** `EnableRawMode()` is called on a Windows 10+ system
- **THEN** the console mode SHALL be set with `ENABLE_VIRTUAL_TERMINAL_PROCESSING` and `ENABLE_VIRTUAL_TERMINAL_INPUT`, and ANSI escape sequences SHALL render correctly

#### Scenario: VT mode fallback
- **WHEN** `SetConsoleMode()` fails to enable VT processing
- **THEN** the terminal SHALL operate in basic line mode without ANSI colors or cursor control

### Requirement: POSIX terminal backend
On POSIX systems, the system SHALL provide a `PosixTerminalIO` implementation that uses `termios` raw mode and `poll()` on file descriptor 0 for non-blocking input.

#### Scenario: Raw mode enabled
- **WHEN** `EnableRawMode()` is called
- **THEN** `termios` SHALL be set to raw mode (no echo, no canonical processing, character-at-a-time)

#### Scenario: Mode restored on shutdown
- **WHEN** `RestoreMode()` is called
- **THEN** the original `termios` settings SHALL be restored

### Requirement: Terminal width query
`ITerminalIO` SHALL provide `GetWidth()` returning the current terminal column count. This SHALL be used for Tab completion layout.

#### Scenario: Get terminal width
- **WHEN** `GetWidth()` is called on an 80-column terminal
- **THEN** it SHALL return `80`

### Requirement: Line editing
`ConsoleTerminal` SHALL support basic line editing: left/right arrow cursor movement, Home/End, Backspace, Delete. The input line SHALL be visually updated in-place using ANSI cursor control sequences.

#### Scenario: Backspace deletes character
- **WHEN** the user has typed `r.shadow` and presses Backspace
- **THEN** the input buffer SHALL become `r.shado` and the terminal display SHALL update accordingly

#### Scenario: Arrow keys move cursor
- **WHEN** the user presses Left arrow
- **THEN** the cursor SHALL move one position left within the input buffer, allowing insertion at that position

### Requirement: Command history navigation
`ConsoleTerminal` SHALL maintain an in-memory history of executed commands. Up/Down arrow keys SHALL cycle through history entries, replacing the current input buffer.

#### Scenario: Navigate to previous command
- **WHEN** the user has executed `r.ShadowQuality 3` and then presses Up arrow
- **THEN** the input buffer SHALL display `r.ShadowQuality 3`

#### Scenario: Down arrow returns to empty input
- **WHEN** the user is at the oldest history entry and presses Down arrow repeatedly until past the newest entry
- **THEN** the input buffer SHALL be empty

### Requirement: Command history persistence
`CommandHistory` SHALL save history to `<project>/.skyengine/console_history` on shutdown and load it on initialization. The file format SHALL be one command per line. The maximum history size SHALL be 1000 entries; older entries SHALL be discarded.

#### Scenario: History persists across sessions
- **WHEN** the engine exits after executing 5 commands, then restarts
- **THEN** pressing Up arrow SHALL show the last command from the previous session

#### Scenario: History file exceeds max size
- **WHEN** the history file contains 1050 entries and is loaded
- **THEN** only the most recent 1000 entries SHALL be loaded

### Requirement: Tab completion lists all matches
When the user presses Tab, `ConsoleTerminal` SHALL call `CommandRegistry::FindByPrefix()` with the current input and display all matching entries in a formatted list showing name, type, and current value (for CVars). The input buffer SHALL NOT be modified.

#### Scenario: Multiple matches
- **WHEN** the user types `r.Shadow` and presses Tab, and three CVars match
- **THEN** all three CVars SHALL be listed below the input line with their types and current values, and the input buffer SHALL remain `r.Shadow`

#### Scenario: No matches
- **WHEN** the user types `zzz` and presses Tab
- **THEN** nothing SHALL be displayed and the input buffer SHALL remain unchanged

#### Scenario: Single match
- **WHEN** the user types `r.Wire` and presses Tab, and only `r.Wireframe` matches
- **THEN** the single match SHALL be listed, and the input buffer SHALL remain `r.Wire`

### Requirement: Log and input interleaving
When log output arrives while the user is typing, `ConsoleTerminal` SHALL: (1) erase the current input line, (2) write the log entry with a newline, (3) redraw the prompt and input buffer with cursor at the correct position. The user's in-progress input SHALL NOT be lost or corrupted.

#### Scenario: Log arrives during typing
- **WHEN** the user has typed `r.shad` and a log entry `[INFO Render] Frame 100` arrives
- **THEN** the log entry SHALL appear on its own line, and the input line `> r.shad` SHALL be redrawn below it with the cursor after `d`

### Requirement: TTY detection
`ConsoleTerminal` SHALL detect whether stdin is a TTY (interactive terminal). If stdin is not a TTY (e.g., piped input), raw mode and line editing SHALL be disabled, and the terminal SHALL operate in line-buffered mode reading complete lines.

#### Scenario: Piped input
- **WHEN** the engine is launched with stdin redirected from a file
- **THEN** the terminal SHALL read complete lines without enabling raw mode, and SHALL NOT output ANSI escape sequences

### Requirement: Log entry formatting
`ConsoleTerminal` SHALL format log entries with ANSI colors: ERROR in red, WARNING in yellow, INFO in default. Format: `[LEVEL TAG] message`. Colors SHALL be omitted when TTY is not detected.

#### Scenario: Error log colored red
- **WHEN** an ERROR log entry is displayed on an interactive terminal
- **THEN** the entry SHALL be wrapped in ANSI red color codes (`\033[31m` ... `\033[0m`)
