## Why

SkyEngine has no runtime command execution system. The only CLI support is `CmdParser` for startup arguments — once the engine is running, there is no way to inspect or modify state interactively. This blocks debugging workflows, headless automation, and runtime tuning. A console system is a foundational primitive that every other subsystem (render, physics, navigation) needs for exposable debug controls.

## What Changes

- Add a **CVar (Console Variable) system** in `engine/core/` — typed, decentralized variables (`CVar<T>`) that any module can declare, with automatic registration into a global `CommandRegistry` singleton. Main-thread-write, any-thread-read.
- Add a **Command function system** in `engine/core/` — stateless commands (`CommandFunc`) registered by name with category/help metadata.
- Add a **CommandShell** in `engine/framework/` — tokenizer, dispatcher, and `exec` file execution for `.cfg` batch files.
- Add a **ConsoleLog** in `engine/framework/` — hooks into `Logger::Print()` to capture log output into a ring buffer, enabling log display in the console terminal.
- Add a **cross-platform system console terminal** in `engine/framework/` — non-blocking stdin/stdout terminal with line editing, command history (persisted to project directory), Tab completion (list all matches), and log/input interleaving via line reprinting. Platform backends: Win32 (`ReadConsoleInput` + VT sequences) and POSIX (`termios` + `poll()`).
- Add a **ConsoleModule** (`IModule`) that drives the terminal in the engine tick loop, syncs `ARCHIVE`-flagged CVars with `SettingRegistry`, and supports `--headless` engine startup.
- Add `--exec <file.cfg>` startup argument support for batch command execution.

## Capabilities

### New Capabilities
- `cvar-registry`: Core CVar<T> and CommandRegistry system — decentralized typed variables, command functions, prefix search, category metadata
- `command-shell`: Command parsing, dispatch, exec file execution, output formatting
- `system-terminal`: Cross-platform non-blocking terminal IO with line editing, Tab completion, history persistence, and log interleaving
- `console-module`: IModule lifecycle, SettingRegistry sync, headless mode, --exec support

### Modified Capabilities
<!-- No existing specs are changing at the requirement level. -->

## Impact

- **engine/core/**: New `console/` directory (ConsoleTypes, CVar, CommandRegistry, CommandToken). Zero changes to existing core code.
- **engine/core/logger/**: ~5-line change to `Logger::Print()` to add an output callback hook for ConsoleLog.
- **engine/framework/**: New `console/` directory (CommandShell, CommandHistory, ConsoleLog, ConsoleModule, terminal/).
- **engine/framework/application/**: ConsoleModule registered in module load configs. Application gains `--headless` and `--exec` argument handling.
- **configs/**: `modules_game.json` and `modules_editor.json` updated to include ConsoleModule.
- **CMakeLists.txt**: Build targets for new source files.
- **Dependencies**: None — pure C++ standard library + existing engine infrastructure (Singleton, Delegate, SettingRegistry, IModule).
