## Context

SkyEngine has a complete console backend (CVar, CommandRegistry, CommandShell, ConsoleLog) and an external terminal frontend (Win32TerminalIO/PosixTerminalIO via ConsoleModule plugin). The external terminal works when a real console window exists, but is invisible or non-interactive in common development scenarios (VS debugger, GUI-only builds). Game engines universally solve this with an in-game overlay console.

The engine already has:
- **ImWidget** + **ImGuiInstance**: Widget-based ImGui rendering with `AddWidget()`/`RemoveWidget()` and per-frame `Execute(ImContext&)` dispatch
- **Interface\<T\>**: Singleton-based service locator for cross-module API discovery
- **Event\<IKeyboardEvent\>**: Input event broadcasting for keyboard capture
- **ConsoleLog**: Thread-safe ring buffer collecting all Logger output
- **CommandShell**: Command parsing, execution, `help`/`find`/`exec` builtins

## Goals / Non-Goals

**Goals:**
- Define `IConsoleUI` in framework/ with zero render dependencies
- Allow any module to register a console UI implementation at runtime via `Interface<IConsoleUI>`
- Provide a default ImGui overlay (UE-style `~` dropdown) in render/imgui/
- Let ConsoleModule drive both the external terminal and overlay console simultaneously
- `WantsInput()` mechanism to suppress game input when the console is open

**Non-Goals:**
- Qt-based editor console panel (future work, same interface)
- In-game console rendering without ImGui (custom draw call backend)
- CVar browser / visual inspector UI
- Console scripting language or Lua integration

## Decisions

### D1: IConsoleUI lives in framework/console/

**Choice**: Pure abstract interface in framework, not in render/ or plugins/.

**Rationale**: framework/ has zero render dependencies and is linked by all higher-level modules. Any module (ImGui render, Qt editor, custom plugin) can implement it. The Interface\<IConsoleUI\> pattern already exists for cross-module service discovery (used by ISystemNotify).

**Alternative considered**: Put in core/ -- rejected because core/ should not contain UI-related abstractions.

### D2: Interface\<IConsoleUI\> for registration

**Choice**: Use the existing `Interface<IConsoleUI>` singleton pattern.

**Rationale**: Exactly matches how `ISystemNotify` is registered. ConsoleModule queries `Interface<IConsoleUI>::Get()->GetApi()` -- if null, no overlay; if set, drive it alongside the terminal. The ImGui module registers its implementation during Init.

**Alternative considered**: Event-based pub/sub -- rejected as overcomplicated for a single-instance service.

### D3: ImGui implementation as ImWidget

**Choice**: `ImGuiConsoleWidget` extends `ImWidget`, added to the active `ImGuiInstance` via `AddWidget()`.

**Rationale**: Follows the exact existing pattern. The widget's `Execute()` draws the console overlay. No new render pipeline plumbing needed.

### D4: Toggle via `~` key event

**Choice**: The ImGui console implementation listens for `IKeyboardEvent` and toggles on grave accent / tilde key.

**Rationale**: ImGuiInstance already implements IKeyboardEvent. The console widget can check for the `~` key in its own keyboard handler or in Execute(). When visible, `WantsInput()` returns true, and the input system skips game bindings.

**Alternative considered**: Configurable key via CVar (e.g. `console.toggleKey`) -- good future enhancement but not needed for v1.

### D5: ConsoleModule drives IConsoleUI::Tick()

**Choice**: In `ConsoleModule::Tick()`, after flushing logs to the terminal, also flush to `IConsoleUI` if registered. The IConsoleUI has its own log buffer and input buffer, independent of the terminal.

**Rationale**: Keeps the two frontends decoupled. Both receive the same ConsoleLog entries. Both can submit commands independently.

## Risks / Trade-offs

- **[Risk] Two frontends submit conflicting commands simultaneously** -- Mitigation: CommandShell is already stateless per-call; concurrent submissions are safe. Log output goes to both.
- **[Risk] ImGui context not available in headless mode** -- Mitigation: IConsoleUI is optional; ConsoleModule checks for null. Headless mode already skips SkyRender.
- **[Risk] Input focus conflict between ImGui widgets and console overlay** -- Mitigation: `WantsInput()` lets the overlay claim keyboard focus; ImGuiInstance can check this before dispatching to other widgets.
- **[Trade-off] IConsoleUI::Render() called by widget, not by ConsoleModule** -- The ImGui widget controls its own rendering via Execute(). ConsoleModule only pushes log entries and polls commands. This means the render timing is owned by the render layer, not the plugin.
