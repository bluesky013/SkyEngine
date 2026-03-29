## Why

The current console system only has an external terminal frontend (`Win32TerminalIO` / `PosixTerminalIO`), which is invisible when debugging from VS or running as a GUI app. Game engines universally provide an in-game overlay console (toggled by `~`). The console backend (CommandShell, CVar, ConsoleLog) is already solid -- what's missing is a render-agnostic UI abstraction that allows multiple frontends (ImGui overlay, Qt panel, custom renderer) to plug in without coupling framework to any rendering library.

## What Changes

- Add `IConsoleUI` interface in `framework/console/` -- pure abstract, zero render dependencies
- Add `IConsoleUIProvider` registration so modules can supply a console UI at runtime
- Add default ImGui implementation as an `ImWidget` in `render/imgui/` (UE-style `~` toggle, log scroll, input box, tab completion popup)
- Modify `ConsoleModule` to discover and drive `IConsoleUI` if one is registered, alongside the existing terminal IO path
- Add `~` (grave accent) key binding to toggle the overlay console via the existing input event system

## Capabilities

### New Capabilities
- `console-ui`: Abstract in-game overlay console interface (`IConsoleUI`) with registration, toggle, input capture, and render contract
- `imgui-console`: Default ImGui-based overlay console implementing `IConsoleUI` -- half-screen dropdown with log area, input field, tab completion, and color-coded log levels

### Modified Capabilities
- `console-module`: ConsoleModule gains awareness of `IConsoleUI` -- drives its Render/Tick alongside the existing terminal path

## Impact

- **framework/console/**: New `IConsoleUI.h` header (interface only, no .cpp needed)
- **render/imgui/**: New `ImGuiConsoleUI.h/.cpp` implementing the ImGui overlay
- **plugins/console/**: `ConsoleModule` updated to query and drive `IConsoleUI`
- **Input system**: `~` key event consumed when console is visible (`WantsInput()`)
- **No breaking changes**: Existing external terminal remains fully functional; `IConsoleUI` is optional
