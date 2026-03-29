## MODIFIED Requirements

### Requirement: Module tick drives terminal
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
