## ADDED Requirements

### Requirement: ConsoleModule IModule lifecycle
`ConsoleModule` SHALL implement `IModule` with `Init()`, `Start()`, `Tick()`, and `Shutdown()` methods. It SHALL be registered via `REGISTER_MODULE` for dynamic loading.

#### Scenario: Module initialization
- **WHEN** `ConsoleModule::Init()` is called
- **THEN** the module SHALL initialize `ConsoleLog`, `CommandShell`, `ConsoleTerminal` (if TTY), register built-in commands, and load command history

#### Scenario: Module tick drives terminal
- **WHEN** `ConsoleModule::Tick()` is called each frame
- **THEN** it SHALL poll terminal input, flush pending log entries to the terminal, and process any completed command lines

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
