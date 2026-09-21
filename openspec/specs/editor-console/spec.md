# editor-console Specification

## Purpose
TBD - created by archiving change editor-shell-redesign. Update Purpose after archive.
## Requirements
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

