## 1. IConsoleUI Interface

- [x] 1.1 Create `framework/console/IConsoleUI.h` with pure virtual interface: Toggle(), Show(), Hide(), IsVisible(), WantsInput(), PushLog(), PollCommand(), PushOutput()
- [x] 1.2 Define LogEntry struct (or reuse existing) accepted by PushLog()
- [x] 1.3 Add `Interface<IConsoleUI>` specialization so modules can register/query via the singleton

## 2. ImGuiConsoleWidget

- [x] 2.1 Create `render/imgui/src/ImGuiConsoleWidget.h/.cpp` extending ImWidget, implementing IConsoleUI
- [x] 2.2 Implement visibility state: Toggle(), Show(), Hide(), IsVisible(), WantsInput()
- [x] 2.3 Implement log ring buffer storage and PushLog() / PushOutput()
- [x] 2.4 Implement PollCommand() with a pending-command queue
- [x] 2.5 Implement Execute(ImContext&): draw semi-transparent dropdown window with scrollable log area and input text field
- [x] 2.6 Color-code log entries by level (red=ERROR, yellow=WARNING, default=INFO/DEBUG)
- [x] 2.7 Implement auto-scroll to newest; preserve position when user scrolls up
- [x] 2.8 Implement command input field with Enter to submit and auto-clear
- [x] 2.9 Implement Up/Down arrow command history navigation via CommandHistory
- [x] 2.10 Implement Tab completion via CommandRegistry::FindByPrefix()
- [x] 2.11 Show completion candidates below input field when multiple matches exist

## 3. Tilde Key Toggle

- [x] 3.1 Add tilde/grave accent key handler in ImGuiConsoleWidget to toggle visibility
- [x] 3.2 Consume the key event so it does not propagate to game input

## 4. Widget Lifecycle and Registration

- [x] 4.1 Register ImGuiConsoleWidget with Interface<IConsoleUI> on construction
- [x] 4.2 Add widget to ImGuiInstance via AddWidget() on creation
- [x] 4.3 Unregister from Interface<IConsoleUI> and RemoveWidget() on destruction
- [x] 4.4 Wire up widget creation/destruction in the ImGui render module init/shutdown path

## 5. ConsoleModule Integration

- [x] 5.1 In ConsoleModule::Tick(), query Interface<IConsoleUI>::Get()->GetApi() each tick
- [x] 5.2 Push flushed ConsoleLog entries to IConsoleUI::PushLog() when overlay is registered
- [x] 5.3 Poll IConsoleUI::PollCommand() and execute results via CommandShell::Execute()
- [x] 5.4 Push command output back via IConsoleUI::PushOutput()
- [x] 5.5 Ensure null-safe: skip overlay path when no IConsoleUI is registered

## 6. Build Integration

- [x] 6.1 Add IConsoleUI.h to framework CMakeLists.txt
- [x] 6.2 Add ImGuiConsoleWidget sources to render CMakeLists.txt
- [x] 6.3 Verify framework builds with no render dependencies from IConsoleUI.h
- [x] 6.4 Build and verify full engine compiles cleanly

## 7. Verification

- [ ] 7.1 Run editor: press `~` to toggle overlay console, verify it renders
- [ ] 7.2 Type a command in the overlay, verify it executes and output appears
- [ ] 7.3 Verify log entries stream into the overlay in real time
- [ ] 7.4 Verify Up/Down history and Tab completion work in the overlay input field
- [ ] 7.5 Verify WantsInput() suppresses game input when console is open
- [ ] 7.6 Verify headless/no-overlay mode works (ConsoleModule with nullptr IConsoleUI)
