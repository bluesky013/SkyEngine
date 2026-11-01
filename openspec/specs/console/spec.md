# console Specification

## Purpose

Define command registration, configuration variables, shell execution and console frontends.

## Requirements

### Requirement: CommandShell executes CVar get

The CommandShell SHALL interpret a single-token input matching a CVar name as a get operation, returning the current value with type and description.

#### Scenario: Query a CVar value
- **WHEN** the user inputs `r.ShadowQuality` (no arguments)
- **THEN** CommandShell SHALL return `CommandResult{OK, "r.ShadowQuality = 2  (int, \"Shadow quality level\")"}` or equivalent formatted output

### Requirement: CommandShell executes CVar set

The CommandShell SHALL interpret a two-token input where the first token matches a CVar name as a set operation, passing the second token to `SetFromString()`.

#### Scenario: Set a CVar value
- **WHEN** the user inputs `r.ShadowQuality 3`
- **THEN** CommandShell SHALL call `SetFromString("3")` and return output like `"r.ShadowQuality = 3 (was: 2)"`

#### Scenario: Set fails due to READ_ONLY
- **WHEN** the user inputs a set command on a READ_ONLY CVar
- **THEN** CommandShell SHALL return `CommandResult{ERROR, "r.BuildVersion is read-only"}`

### Requirement: CommandShell executes command functions

The CommandShell SHALL interpret the first token as a command name lookup. If found, the remaining tokens are passed as `CommandArgs` to the command handler.

#### Scenario: Execute help command
- **WHEN** the user inputs `help r`
- **THEN** the `help` command handler SHALL receive args `["r"]` and return a listing of all CVars/commands in category `"r"`

### Requirement: CommandShell reports unknown commands

If the first token matches neither a CVar name nor a registered command, CommandShell SHALL return `CommandResult{NOT_FOUND, "Unknown command: <name>"}`.

#### Scenario: Unknown command
- **WHEN** the user inputs `foobar 123`
- **THEN** CommandShell SHALL return `{NOT_FOUND, "Unknown command: foobar"}`

### Requirement: CommandShell exec file

The CommandShell SHALL provide `ExecFile(path)` which reads a `.cfg` file and executes each line as a command. Lines starting with `//` (after optional whitespace) SHALL be skipped as comments. Empty or whitespace-only lines SHALL be skipped.

#### Scenario: Execute a cfg file
- **WHEN** `ExecFile("startup.cfg")` is called and the file contains:
  ```
  // shadow setup
  r.ShadowQuality 3
  r.ShadowDistance 150.0

  // debug
  r.Wireframe 0
  ```
- **THEN** three commands SHALL be executed and the result SHALL report `"Executed 3 commands from startup.cfg"`

#### Scenario: Cfg file not found
- **WHEN** `ExecFile("nonexistent.cfg")` is called
- **THEN** the result SHALL be `CommandResult{ERROR, "File not found: nonexistent.cfg"}`

### Requirement: Output formatting

CommandShell SHALL format output as plain text. CVar query output SHALL include name, current value, type, and description. Error output SHALL be prefixed for clear identification.

#### Scenario: CVar query format
- **WHEN** querying a float CVar with value 100.0 and description "Shadow draw distance"
- **THEN** the output SHALL contain the CVar name, value `100`, type `float`, and description text

### Requirement: Built-in help command

CommandShell SHALL register a `help` command. When called with no arguments, it SHALL list all non-hidden CVars and commands. When called with a category prefix, it SHALL list only entries in that category.

#### Scenario: Help with category filter
- **WHEN** the user inputs `help phys`
- **THEN** the output SHALL list only CVars/commands whose category is `"phys"`

#### Scenario: Help with specific command name
- **WHEN** the user inputs `help sys.exit`
- **THEN** the output SHALL show the description for `sys.exit`

### Requirement: Built-in find command

CommandShell SHALL register a `find` command that searches CVar/command names and descriptions by substring match (case-insensitive).

#### Scenario: Find by substring
- **WHEN** the user inputs `find shadow`
- **THEN** the output SHALL list all CVars/commands containing "shadow" in their name or description

### Requirement: Built-in reset command

CommandShell SHALL register a `reset` command that restores a CVar to its default value.

#### Scenario: Reset a CVar
- **WHEN** the user inputs `reset r.ShadowQuality` and the default is 2
- **THEN** the CVar SHALL be set to 2 and the output SHALL indicate the reset

### Requirement: Built-in echo command

CommandShell SHALL register an `echo` command that concatenates its arguments with spaces and returns them as output.

#### Scenario: Echo text
- **WHEN** the user inputs `echo hello world`
- **THEN** the output SHALL be `"hello world"`

### Requirement: ConsoleModule IModule lifecycle

`ConsoleModule` SHALL implement `IModule` with `Init()`, `Start()`, `Tick()`, and `Shutdown()` methods. It SHALL be registered via `REGISTER_MODULE` for dynamic loading.

#### Scenario: Module initialization
- **WHEN** `ConsoleModule::Init()` is called
- **THEN** the module SHALL initialize `ConsoleLog`, `CommandShell`, `ConsoleTerminal` (if TTY), register built-in commands, and load command history

#### Scenario: Module tick drives terminal
- **WHEN** `ConsoleModule::Tick()` is called each frame
- **THEN** it SHALL poll terminal input, flush pending log entries to the terminal, process any completed command lines, AND if an `IConsoleUI` is registered via `Interface<IConsoleUI>`, it SHALL also push log entries to the overlay and poll/execute overlay commands

#### Scenario: Overlay receives log entries
- **WHEN** `ConsoleModule::Tick()` flushes log entries and `Interface<IConsoleUI>::Get()->GetApi()` returns non-null
- **THEN** each flushed `LogEntry` SHALL be passed to `IConsoleUI::PushLog()`

#### Scenario: Overlay commands executed
- **WHEN** `IConsoleUI::PollCommand()` returns true during tick
- **THEN** the command SHALL be executed via `CommandShell::Execute()` and the result pushed back via `IConsoleUI::PushOutput()`

#### Scenario: No overlay registered
- **WHEN** `Interface<IConsoleUI>::Get()->GetApi()` returns nullptr
- **THEN** `ConsoleModule::Tick()` SHALL behave exactly as before (terminal-only path)

#### Scenario: Both terminal and overlay active
- **WHEN** both the external terminal and an `IConsoleUI` overlay are active
- **THEN** log entries SHALL be delivered to both, and commands from either source SHALL be executed independently

#### Scenario: Module shutdown saves state
- **WHEN** `ConsoleModule::Shutdown()` is called
- **THEN** it SHALL save command history, sync ARCHIVE CVars to SettingRegistry, restore terminal mode, and deregister the Logger callback

### Requirement: ConsoleLog captures Logger output

`ConsoleLog` SHALL set a callback on `Logger::Print()` that captures log entries into a thread-safe ring buffer. The ring buffer SHALL hold at least 2048 entries.

#### Scenario: Log entry captured
- **WHEN** `LOG_I("Render", "Frame %d", 100)` is called from any thread
- **THEN** `ConsoleLog` SHALL store a `LogEntry{tag="Render", level="INFO", message="Frame 100", timestamp=...}` in the ring buffer

#### Scenario: Ring buffer overflow
- **WHEN** the ring buffer is full and a new entry arrives
- **THEN** the oldest entry SHALL be overwritten

### Requirement: ConsoleLog thread-safe flush

`ConsoleLog` SHALL provide a `FlushPendingEntries(callback)` method that dequeues entries added since the last flush and calls the callback for each. This method SHALL be called from the main thread during `ConsoleModule::Tick()`.

#### Scenario: Flush delivers entries in order
- **WHEN** three log entries are captured and `FlushPendingEntries()` is called
- **THEN** the callback SHALL be invoked three times in chronological order

### Requirement: SettingRegistry CVar sync

On initialization, `ConsoleModule` SHALL iterate all CVars with the `ARCHIVE` flag and load their values from `SettingRegistry` (if a matching key exists). On shutdown, it SHALL write all `ARCHIVE` CVar current values back to `SettingRegistry`.

#### Scenario: Load archived CVar from settings
- **WHEN** `SettingRegistry` contains key `"r.ShadowQuality"` with value `"3"` and a CVar `r.ShadowQuality` with `ARCHIVE` flag exists
- **THEN** `ConsoleModule::Init()` SHALL call `SetFromString("3")` on that CVar

#### Scenario: Save archived CVar on shutdown
- **WHEN** the user changed `r.ShadowQuality` to 3 during the session and the engine shuts down
- **THEN** `ConsoleModule::Shutdown()` SHALL write `"r.ShadowQuality" = "3"` to `SettingRegistry`

### Requirement: Headless mode support

When the engine is started with `--headless`, `ConsoleModule` SHALL still initialize fully. The system console terminal SHALL be the primary user interface. No window or rendering modules SHALL be required.

#### Scenario: Headless launch
- **WHEN** the engine is started with `--headless`
- **THEN** `ConsoleModule` SHALL initialize, the console prompt SHALL appear on stdout, and the engine SHALL enter its main loop without creating a window

### Requirement: --exec startup argument

The engine SHALL support `--exec <path>` as a startup argument. `ConsoleModule` SHALL call `CommandShell::ExecFile(path)` during `Start()` after all modules have initialized.

#### Scenario: Execute startup config
- **WHEN** the engine is started with `--exec config/startup.cfg`
- **THEN** all commands in `startup.cfg` SHALL be executed before the first `Tick()` call

#### Scenario: Multiple --exec arguments
- **WHEN** the engine is started with `--exec a.cfg --exec b.cfg`
- **THEN** `a.cfg` SHALL be executed first, then `b.cfg`

### Requirement: sys.exit command

`ConsoleModule` SHALL register a `sys.exit` command that calls `Application::SetExit()` to trigger graceful engine shutdown.

#### Scenario: Exit via console
- **WHEN** the user types `sys.exit`
- **THEN** the engine SHALL begin its shutdown sequence

### Requirement: sys.version command

`ConsoleModule` SHALL register a `sys.version` command that prints the engine name and build information.

#### Scenario: Version query
- **WHEN** the user types `sys.version`
- **THEN** the output SHALL contain the engine name "SkyEngine"

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

### Requirement: CVar typed declaration and auto-registration

The system SHALL provide a `CVar<T>` template class supporting types: `bool`, `int`, `float`, `double`, `std::string`. A `CVar<T>` instance SHALL automatically register itself with `CommandRegistry` upon construction and unregister upon destruction.

#### Scenario: Declare a CVar as a static variable
- **WHEN** a module declares `static CVar<int> r_shadowQuality("r.ShadowQuality", 2, "Shadow quality level")`
- **THEN** `CommandRegistry::Get()->FindCVar("r.ShadowQuality")` SHALL return a non-null `ICVar*` pointing to that CVar

#### Scenario: CVar destruction triggers unregistration
- **WHEN** a CVar goes out of scope or its owning module is unloaded
- **THEN** `CommandRegistry::Get()->FindCVar()` for that name SHALL return `nullptr`

### Requirement: CVar value storage is decentralized

Each `CVar<T>` SHALL store its value directly in its own memory. `CVar<T>::Get()` SHALL return a const reference to the local value with no indirection through the registry.

#### Scenario: Hot-path read performance
- **WHEN** code calls `cvar.Get()` in a per-frame loop
- **THEN** the read SHALL resolve to a direct memory access with no map lookup or pointer chase through CommandRegistry

### Requirement: CVar main-thread-write contract

`CVar<T>::Set()` and `CVar<T>::SetFromString()` SHALL only be called from the main thread. `CVar<T>::Get()` MAY be called from any thread.

#### Scenario: Set from console input on main thread
- **WHEN** the user types `r.ShadowQuality 3` in the console and CommandShell processes it during `ConsoleModule::Tick()`
- **THEN** `CVar::SetFromString("3")` is called on the main thread, and `Get()` returns 3 afterward

### Requirement: CVar flags

Each CVar SHALL support a `CVarFlags` bitmask with at least: `NONE`, `READ_ONLY`, `CHEAT`, `ARCHIVE`, `HIDDEN`, `REQUIRES_RESTART`.

#### Scenario: READ_ONLY CVar rejects writes
- **WHEN** `SetFromString()` is called on a CVar with `READ_ONLY` flag
- **THEN** the call SHALL return `false` and the value SHALL remain unchanged

#### Scenario: HIDDEN CVar excluded from Tab completion
- **WHEN** `CommandRegistry::FindByPrefix()` is called
- **THEN** CVars with the `HIDDEN` flag SHALL NOT appear in the results

### Requirement: CVar onChange callback

Each `CVar<T>` SHALL expose an `onChange` delegate of type `Delegate<void(const T& oldVal, const T& newVal)>`. The delegate SHALL be invoked whenever `Set()` or `SetFromString()` successfully changes the value.

#### Scenario: Render system reacts to CVar change
- **WHEN** a CVar's value changes from 2 to 3
- **THEN** the connected `onChange` delegate SHALL be called with `oldVal=2, newVal=3`

#### Scenario: No callback on same-value set
- **WHEN** `Set()` is called with the same value as current
- **THEN** the `onChange` delegate SHALL NOT be invoked

### Requirement: CVar string conversion

`ICVar` SHALL provide `ToString()` and `SetFromString()` methods. `ToString()` SHALL produce a human-readable representation. `SetFromString()` SHALL parse the string and return `true` on success, `false` on parse failure.

#### Scenario: Bool CVar accepts multiple representations
- **WHEN** `SetFromString("true")`, `SetFromString("1")`, or `SetFromString("on")` is called on a `CVar<bool>`
- **THEN** each SHALL set the value to `true` and return `true`

#### Scenario: Invalid string for int CVar
- **WHEN** `SetFromString("abc")` is called on a `CVar<int>`
- **THEN** the call SHALL return `false` and the value SHALL remain unchanged

### Requirement: CVar metadata

Each `ICVar` SHALL expose: `GetName()`, `GetDesc()`, `GetCategory()` (derived from dot prefix, e.g. `"r"` from `"r.ShadowQuality"`), `GetTypeName()` (e.g. `"int"`, `"float"`, `"bool"`, `"string"`), and `GetFlags()`.

#### Scenario: Category derived from name
- **WHEN** a CVar is registered with name `"phys.Gravity"`
- **THEN** `GetCategory()` SHALL return `"phys"`

#### Scenario: Name without dot has empty category
- **WHEN** a CVar is registered with name `"debugMode"`
- **THEN** `GetCategory()` SHALL return `""`

### Requirement: CommandRegistry singleton

`CommandRegistry` SHALL be a `Singleton<CommandRegistry>`. It SHALL provide: `RegisterCVar()`, `UnregisterCVar()`, `RegisterCommand()`, `UnregisterCommand()`, `FindCVar()`, `FindCommand()`, `FindByPrefix()`, `ForEachCVar()`, `ForEachCommand()`.

#### Scenario: Register and find a command function
- **WHEN** `RegisterCommand("sys.exit", "Exit engine", "sys", handler)` is called
- **THEN** `FindCommand("sys.exit")` SHALL return the registered command entry

#### Scenario: Prefix search
- **WHEN** three CVars `r.ShadowQuality`, `r.ShadowDistance`, `r.Wireframe` are registered and `FindByPrefix("r.Shadow")` is called
- **THEN** the result SHALL contain exactly `"r.ShadowQuality"` and `"r.ShadowDistance"`

### Requirement: Command function registration

The system SHALL support registering stateless command functions with `RegisterCommand(name, description, category, CommandFunc)` where `CommandFunc = std::function<CommandResult(CommandArgs)>`.

#### Scenario: Execute a registered command
- **WHEN** `FindCommand("echo")` returns a valid entry and its handler is called with args `["hello", "world"]`
- **THEN** the handler SHALL return `CommandResult{Status::OK, "hello world"}`

### Requirement: CommandToken tokenizer

The system SHALL provide a tokenizer that splits a command string into tokens. Tokens are separated by whitespace. Quoted strings (double quotes) SHALL be treated as a single token.

#### Scenario: Simple tokenization
- **WHEN** tokenizing `r.ShadowQuality 3`
- **THEN** the result SHALL be `["r.ShadowQuality", "3"]`

#### Scenario: Quoted string token
- **WHEN** tokenizing `echo "hello world"`
- **THEN** the result SHALL be `["echo", "hello world"]`

### Requirement: Log pipeline into the editor

The editor SHALL surface engine logs in an Output Log panel by piping the logger through the console subsystem
(`ConsoleLog`) and draining pending entries on the main thread. The log view SHALL be a `sky::ui` view and SHALL
not require a UI toolkit in the model.

#### Scenario: Log entries appear
- **WHEN** the engine emits a log message after the editor has installed the log sink
- **THEN** the entry SHALL be delivered to the Output Log view on the main thread

#### Scenario: Bounded history
- **WHEN** more log entries are produced than the ring capacity
- **THEN** the oldest entries SHALL be discarded and the view SHALL remain bounded

### Requirement: Log view controls

The Output Log panel SHALL support level/tag filtering, text search, clearing, auto-scroll, and copying the
visible text to the clipboard.

#### Scenario: Filter and search
- **WHEN** a level/tag filter or a search term is applied
- **THEN** only matching entries SHALL be displayed

#### Scenario: Clear
- **WHEN** the log is cleared
- **THEN** the displayed transcript SHALL be empty

### Requirement: Command line

The editor SHALL provide a command-line console that executes input through the console subsystem
(`CommandShell`) and displays the result, with command history and completion.

#### Scenario: Execute a command
- **WHEN** a command line is submitted
- **THEN** it SHALL be executed through `CommandShell` and the result SHALL be appended to the transcript

#### Scenario: History and completion
- **WHEN** the user navigates history or requests completion
- **THEN** previous commands or matching command names SHALL be offered via `CommandHistory` / `CommandRegistry`

### Requirement: Console panels are dockable and toolkit-independent

The Output Log and Console SHALL be panels registered in the panel registry/layout and SHALL report whether they
want input so editor input routing can gate the viewport.

#### Scenario: Registered panels
- **WHEN** the layout is built
- **THEN** the Output Log and Console SHALL be resolvable as registered panels

#### Scenario: Input gating
- **WHEN** the console input line has focus
- **THEN** the console SHALL report that it wants input and the viewport SHALL not receive that input

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

### Requirement: imgui-console: Command history navigation

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

### Requirement: system-terminal: Command history navigation

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
