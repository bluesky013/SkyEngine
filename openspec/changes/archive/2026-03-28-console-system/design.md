## Context

SkyEngine is a cross-platform C++ game engine (Windows/macOS/Linux/Android/iOS) with a module-based architecture. Key existing infrastructure:

- **Singleton<T>** + **Environment**: Global service registry with lazy initialization and cross-DLL sharing.
- **Event<T>** / **Delegate<T>**: Observer pattern and single-function callbacks.
- **IModule** + **ModuleManager**: Plugin lifecycle with topological dependency ordering (Init → Start → Tick → Shutdown).
- **SettingRegistry**: JSON-backed persistent settings store (RapidJSON).
- **Logger::Print()**: Printf-style logging with tag + level, no output callback hook.
- **CmdParser**: One-shot argc/argv parser for startup arguments.
- **ImWidget** + **ImGuiInstance**: ImGui widget system with per-window instances (Phase 2, not this change).

There is currently no runtime command execution, no CVar system, and no interactive console. Developers must recompile to change debug parameters.

## Goals / Non-Goals

**Goals:**
- Typed CVar system usable from any module with zero-overhead reads
- Command function registration for stateless actions
- Interactive system console terminal (stdin/stdout) with line editing, Tab completion, history
- Cross-platform terminal IO (Windows + POSIX)
- Non-blocking integration into engine main loop
- Headless engine startup mode driven entirely by console
- `.cfg` batch file execution
- ARCHIVE CVar ↔ SettingRegistry bidirectional sync

**Non-Goals:**
- ImGui in-game console (Phase 2)
- Qt editor console panel (Phase 2)
- Remote TCP/WebSocket console server (Phase 2)
- Scripting language integration (Python bindings deferred)
- Undo/Redo for CVar changes
- Conditional logic or variables in `.cfg` files
- CVar replication/networking

## Decisions

### D1: CVar storage — Decentralized (value in CVar<T> instance)

**Choice**: Each `CVar<T>` owns its value directly. The `CommandRegistry` holds `ICVar*` pointers, not values.

**Alternatives considered**:
- *Centralized map*: All values in a `std::unordered_map<string, Variant>` inside Registry. Rejected because it adds indirection on every read — a CVar like `r_wireframe` may be read every frame in hot rendering code.

**Rationale**: `cvar.Get()` compiles to a direct memory load. Registration is a one-time cost at static init. Deregistration happens at static destruction (or module unload).

### D2: Thread safety — Main-thread-write only

**Choice**: `CVar::Set()` and `SetFromString()` are only called from the main thread. Other threads may call `Get()` freely. No atomic/mutex on the value.

**Alternatives considered**:
- *std::atomic<T>*: Works for POD but not `std::string`. Adds complexity.
- *Double-buffer per frame*: Overkill — CVars change rarely (user interaction or config load).

**Rationale**: The console terminal runs in `ConsoleModule::Tick()` on the main thread. Config loading happens during `Init()` on the main thread. There is no legitimate write path from another thread. Render thread reads are safe because CVar writes are infrequent and non-atomic tearing of a `float` or `int` is benign on all target platforms (x86/ARM aligned loads are atomic at word granularity).

### D3: Terminal IO abstraction — Platform-specific backends behind ITerminalIO

**Choice**: Abstract `ITerminalIO` interface with `PollChar()` (non-blocking), `Write()`, `GetWidth()`, `EnableRawMode()`, `RestoreMode()`. Two implementations:
- `Win32TerminalIO`: `PeekConsoleInput()` + `ReadConsoleInput()` + VT sequence output via `SetConsoleMode(ENABLE_VIRTUAL_TERMINAL_PROCESSING)`.
- `PosixTerminalIO`: `termios` raw mode + `poll()` on fd 0.

**Rationale**: The engine already uses compile-time platform branching (`SKY_PLATFORM_WINDOWS`). Non-blocking read is essential — the engine tick must not stall waiting for input.

### D4: Log/input interleaving — Line reprinting

**Choice**: When a log entry arrives while the user is typing:
1. `\r\033[K` — carriage return + erase line (clear the input line)
2. Write the log entry + newline
3. Redraw prompt + input buffer + reposition cursor

**Rationale**: This is the standard approach used by GDB, Redis CLI, and similar interactive programs that share stdout with async output. ANSI escape sequences work on both Windows 10+ (VT mode) and all POSIX terminals.

### D5: Tab completion — List all matches

**Choice**: Single Tab press lists all CVars/commands matching the current prefix, formatted as a table with name, type, and current value. Input buffer is not modified.

**Alternatives considered**:
- *Longest common prefix completion*: More efficient when there's a unique match, but less discoverable.
- *Inline ghost suggestion*: Complex to implement correctly with ANSI sequences.

**Rationale**: Listing all matches is simplest and most informative. Users can see the full option space and continue typing to narrow down.

### D6: Command history — Project-directory persistence

**Choice**: History saved to `<project>/.skyengine/console_history`, one command per line, max 1000 entries. Loaded on `ConsoleModule::Init()`, saved on `Shutdown()`.

**Rationale**: Project-scoped history is more useful than global — different projects have different CVars. The `.skyengine/` directory is already a natural place for per-project engine state.

### D7: Logger hook — Callback function pointer in Logger

**Choice**: Add a `static std::function<void(const char*, const char*, const char*)> Logger::sOutputCallback` that, when set, is called from `Logger::Print()` with (tag, type, message). ConsoleLog sets this during Init.

**Alternatives considered**:
- *Subclass Logger*: Logger is not virtual, too invasive.
- *Redirect stdout*: Lossy, doesn't capture structured tag/level.
- *Ring buffer polled from Logger*: Adds coupling in wrong direction.

**Rationale**: Minimal change (~5 lines in Logger.cpp). The callback is null by default so zero cost when no console is active.

### D8: cfg file format — Minimal, no scripting

**Choice**: Each line is one command. `//` starts a line comment. Empty/whitespace-only lines are skipped. No variables, conditionals, or includes.

**Rationale**: Simple to implement (< 30 lines), covers the primary use case (batch configuration). Complex logic belongs in Python scripts via the existing PythonModule.

### D9: Headless mode — ConsoleModule drives the loop

**Choice**: `--headless` flag skips window/render module loading. ConsoleModule still initializes terminal IO. The engine runs its Mainloop with console as the only interactive interface.

**Rationale**: Enables CI/CD, automated testing, dedicated servers. ConsoleModule has no dependencies on render/window.

## Risks / Trade-offs

- **[Static init order]** CVar global/static variables register during static init. → Mitigation: `CommandRegistry::Get()` uses Singleton lazy init, so it's created on first access regardless of init order. Registration is just pointer insertion into a map.

- **[Module unload]** If a plugin DLL is unloaded, its CVar static variables are destroyed, calling `UnregisterCVar()`. → Mitigation: Standard C++ DLL unload ordering. The `ICVar*` pointer is removed from the Registry map before the memory is freed.

- **[Windows legacy console]** Windows 7 doesn't support VT sequences. → Mitigation: Engine requires Windows 10+. If VT mode fails to enable, fall back to basic line-mode input (no colors, no line reprinting — just readline-style).

- **[Terminal not available]** Some launch contexts (Windows subsystem app, redirected pipes) have no interactive terminal. → Mitigation: `ConsoleModule` detects if stdin is a TTY. If not, disables raw mode and line editing, operates in pipe-friendly mode (read lines, write plain text).

- **[Performance of prefix search]** `FindByPrefix()` iterates all entries. → Mitigation: Typically < 200 CVars/commands total. Linear scan is sub-microsecond. If it ever becomes a bottleneck, swap to a trie — but this is extremely unlikely.
