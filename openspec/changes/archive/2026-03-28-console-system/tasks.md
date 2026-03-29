## 1. Core Types and CVar System (engine/core/console/)

- [x] 1.1 Create `ConsoleTypes.h` — define `CVarFlags` enum (NONE, READ_ONLY, CHEAT, ARCHIVE, HIDDEN, REQUIRES_RESTART), `CommandResult` struct (Status enum + output string), `CommandArgs` type alias (`std::span<const std::string_view>`), `CommandFunc` type alias
- [x] 1.2 Create `CommandToken.h/.cpp` — implement tokenizer: split input string into tokens by whitespace, handle double-quoted strings as single tokens
- [x] 1.3 Create `ICVar` abstract base class in `CVar.h` — virtual interface with `GetName()`, `GetDesc()`, `GetCategory()`, `GetTypeName()`, `GetFlags()`, `ToString()`, `SetFromString()`, `ResetToDefault()`
- [x] 1.4 Create `CVar<T>` template in `CVar.h` — implement decentralized storage, auto-register on construction, auto-unregister on destruction, `Get()` returns const ref to local value, `Set()` triggers `onChange` delegate only when value changes, `SetFromString()`/`ToString()` with type-specific parsing (bool accepts true/false/1/0/on/off), READ_ONLY rejection
- [x] 1.5 Create `CommandRegistry.h/.cpp` — Singleton with `RegisterCVar()`, `UnregisterCVar()`, `RegisterCommand()`, `UnregisterCommand()`, `FindCVar()`, `FindCommand()`, `FindByPrefix()` (excludes HIDDEN), `ForEachCVar()`, `ForEachCommand()`
- [x] 1.6 Add CMakeLists.txt entries for core/console/ source files
- [x] 1.7 Write unit tests for CommandToken (simple split, quoted strings, empty input, edge cases)
- [x] 1.8 Write unit tests for CVar<T> (registration, Get/Set, SetFromString for all types, READ_ONLY, onChange, category parsing, ToString)
- [x] 1.9 Write unit tests for CommandRegistry (register/unregister, find, prefix search, HIDDEN exclusion)

## 2. Logger Hook (engine/core/logger/)

- [x] 2.1 Add `static std::function<void(const char* tag, const char* type, const char* message)> sOutputCallback` to Logger class and call it from `Logger::Print()` when non-null (~5 lines)

## 3. Command Shell (engine/framework/console/)

- [x] 3.1 Create `CommandShell.h/.cpp` — `Execute(string_view)` method: tokenize, lookup first token as CVar (get/set) or command, dispatch, return `CommandResult`. Implement CVar get (show value+type+desc), CVar set (show old→new), command dispatch with args
- [x] 3.2 Implement `CommandShell::ExecFile(path)` — read file line by line, skip `//` comments and empty lines, execute each line, return summary result
- [x] 3.3 Register built-in commands in CommandShell: `help` (list all or filter by category/name), `find` (substring search), `reset` (restore CVar default), `echo` (concatenate args), `exec` (delegate to ExecFile)
- [x] 3.4 Create `CommandHistory.h/.cpp` — in-memory vector with cursor for Up/Down navigation, `Add()`, `GetPrevious()`, `GetNext()`, `ResetCursor()`, `Save(path)`, `Load(path)` (one line per entry, max 1000)
- [x] 3.5 Create `ConsoleLog.h/.cpp` — ring buffer of `LogEntry` structs (tag, level, message, timestamp), Logger callback hook, `FlushPendingEntries(callback)` (thread-safe: log-producing threads write via lock, main thread reads via flush)
- [x] 3.6 Add CMakeLists.txt entries for framework/console/ source files
- [x] 3.7 Write unit tests for CommandShell (CVar get/set dispatch, command dispatch, unknown command, READ_ONLY error)
- [x] 3.8 Write unit tests for ExecFile (comment skipping, empty lines, valid file, missing file)
- [x] 3.9 Write unit tests for CommandHistory (add, navigate up/down, save/load round-trip, max size truncation)
- [x] 3.10 Write unit tests for ConsoleLog (capture entries, flush ordering, ring buffer overflow)

## 4. Cross-Platform Terminal IO (engine/framework/console/terminal/)

- [x] 4.1 Create `ITerminalIO.h` — abstract interface: `PollChar(char&)`, `Write(string_view)`, `GetWidth()`, `EnableRawMode()`, `RestoreMode()`, `IsTTY()`
- [x] 4.2 Create `Win32TerminalIO.cpp` — implement using `PeekConsoleInput()`/`ReadConsoleInput()` for non-blocking input, `SetConsoleMode()` with `ENABLE_VIRTUAL_TERMINAL_PROCESSING`/`ENABLE_VIRTUAL_TERMINAL_INPUT` for VT output, fallback detection, `GetConsoleScreenBufferInfo()` for width
- [x] 4.3 Create `PosixTerminalIO.cpp` — implement using `termios` raw mode, `poll()` on fd 0 with 0ms timeout, `ioctl(TIOCGWINSZ)` for width, save/restore original termios
- [x] 4.4 Create `ConsoleTerminal.h/.cpp` — line editing engine: input buffer + cursor position, handle printable chars, Backspace, Delete, Home, End, Left/Right arrows, Enter (submit line). Decode platform key events (VT escape sequences for arrows: `\033[A/B/C/D`)
- [x] 4.5 Implement Tab completion in ConsoleTerminal — on Tab keypress: call `CommandRegistry::FindByPrefix()`, format matches as table (name, type, value), write below input line, redraw input line unchanged
- [x] 4.6 Implement history navigation in ConsoleTerminal — Up/Down arrows call `CommandHistory::GetPrevious()`/`GetNext()`, replace input buffer, redraw
- [x] 4.7 Implement log/input interleaving in ConsoleTerminal — `WriteLogEntry()`: erase current input line (`\r\033[K`), write formatted log entry with ANSI colors (red=ERROR, yellow=WARNING), redraw prompt + input buffer + cursor
- [x] 4.8 Implement TTY detection — if `IsTTY()` returns false, disable raw mode, read complete lines via standard getline, omit ANSI escape sequences
- [x] 4.9 Write integration test for ConsoleTerminal with mock ITerminalIO (simulate key input sequence, verify output)

## 5. Console Module (engine/framework/console/)

- [x] 5.1 Create `ConsoleModule.h/.cpp` — implement `IModule`: `Init()` creates ConsoleLog/CommandShell/ConsoleTerminal, registers Logger callback, loads command history, registers built-in commands (`sys.exit`, `sys.version`). `Tick()` polls terminal input and flushes log entries. `Shutdown()` saves history, syncs ARCHIVE CVars to SettingRegistry, restores terminal mode
- [x] 5.2 Implement ARCHIVE CVar sync — on Init: iterate registered CVars with ARCHIVE flag, read from SettingRegistry, call SetFromString. On Shutdown: iterate ARCHIVE CVars, write ToString() back to SettingRegistry
- [x] 5.3 Implement `--exec` argument handling — parse from StartArguments in Init/Start, call ExecFile for each --exec path in order
- [x] 5.4 Register ConsoleModule with `REGISTER_MODULE` macro
- [x] 5.5 Add ConsoleModule to `configs/modules_game.json` and `configs/modules_editor.json`
- [x] 5.6 Update framework CMakeLists.txt with console module build target and link dependencies

## 6. Headless Mode

- [x] 6.1 Add `--headless` flag parsing in Application — set a flag indicating headless mode, skip window/render module loading when set
- [x] 6.2 Verify ConsoleModule works standalone without render/window modules — test headless launch with `--headless --exec test.cfg`

## 7. End-to-End Integration

- [x] 7.1 Create a sample `configs/default_console.cfg` with example CVar settings and comments demonstrating cfg format
- [x] 7.2 Add integration test: register CVars, execute commands via CommandShell, verify state changes
- [x] 7.3 Manual smoke test: build and run engine, interact with console (type commands, Tab complete, history navigation, exec file, sys.exit)
