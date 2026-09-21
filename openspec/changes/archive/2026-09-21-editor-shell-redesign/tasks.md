Legend: `[headless]` needs no window/render; `[render]` needs the Aurora UI pass; `[external]` blocked by another change.

## 1. Module scaffolding

- [x] 1.1 Create `engine/sandbox/core` (`EditorCore` STATIC, links `Core` + `Framework`) with a configure-time guard rejecting `aurora/`, `render/`, `ui/`, and Qt includes in public headers
- [x] 1.2 Create `engine/sandbox/src` (`Sandbox` EXE, opt-in via `SKY_BUILD_SANDBOX`) and `engine/sandbox/test` (`EditorCoreTest`, via `SKY_BUILD_TEST`)
- [x] 1.3 Wire the build: `SKY_BUILD_SANDBOX` option; `engine/sandbox/CMakeLists.txt` (core + shell + test) and `engine/CMakeLists.txt` (sandbox under SANDBOX OR TEST). Legacy `engine/editor` is not modified.
- [x] 1.4 Sandbox opens a non-Qt native window via `Platform` + `NativeWindow` and pumps events; verify configure, build, and run (no RHI dependency)

## 2. Core service: undo / redo (`editor-undo-redo`)

- [x] 2.1 Define `UndoCommand` (`Do`/`Undo`) and a `CommandService` in `EditorCore` with `Execute`, `Undo`, `Redo`, `CanUndo`/`CanRedo`
- [x] 2.2 Add transaction grouping (`BeginTransaction`/`EndTransaction`) so multiple commands undo as one step
- [x] 2.3 Add the change notification/observer so views can update enabled state
- [x] 2.4 Add the reflection-driven generic `PropertyEditCommand` (object handle, member path, old/new value)

## 3. Core service: property model (`editor-property-model`)

- [x] 3.1 Define `PropertyDescriptor` over `serialize::TypeMemberNode` (name/display name/category/type/metadata, `Get`/`Set`)
- [x] 3.2 Add child descriptors for sequence elements and expose struct member `TypeNode`s (associative maps out of scope)
- [x] 3.3 Route descriptor writes through the undo service as a `PropertyEditCommand`
- [x] 3.4 Ensure the model has no toolkit/render includes (guard) and expose it via `EditorCore`

## 4. Core services: documents and selection (`editor-document`, `editor-selection`)

- [x] 4.1 Define the document service (open/load/save/dirty) with an asset-to-document mapping
- [x] 4.2 Define the observable selection/editor-context service as the single owner of selection
- [x] 4.3 Provide subscribe/notify for selection changes (outliner/inspector/viewport adopt it in Phase 3)

## 5. Toolkit-independent extension registration (`editor-core`)

- [x] 5.1 Move the registration API (asset creators, actor creators) into `EditorCore`; gizmo factory registration already lives in toolkit-agnostic `Framework`
- [x] 5.2 `[headless]` Register extensions through the core API (`EditorExtension` + `EditorExtensionHost`, add/register/unregister); validated with a dummy extension. DLL module loading reuses the Framework `ModuleManager`.
- [ ] 5.3 `[render]` Remove the Qt/`EditorFramework` dependency from `AuroraRender.Editor` so it can register through the sandbox core

## 6. Headless tests for core services (`editor-core`)

- [x] 6.1 Add `engine/sandbox/test` (`EditorCoreTest`) and register it under `SKY_BUILD_TEST`
- [x] 6.2 Tests: undo/redo, transaction grouping, property read/write + undo, document dirty/save round-trip, selection notify
- [x] 6.3 Run the tests headless (no window, no GPU)

## 7. Native application shell (`editor-application`)

- [x] 7.1 Create the editor application target using `Platform` + `NativeWindow` and the module/frame driver, with no UI toolkit in the app layer
- [x] 7.2 Provide shell services through the platform layer: clipboard + open/save file dialogs (Win32 backend)
- [x] 7.3 Wire text input: `Platform` `StartTextInput`/`StopTextInput`/`SetTextInputRect` + SDL backend (full CJK preedit polish deferred)
- [ ] 7.4 `[headless]` macOS application/global menu (native); confirm Windows needs no native menu bar (the editor menu bar itself is engine-drawn, 10.2)
- [x] 7.5 `[headless]` Add a user-config directory API to `Platform` (SDL_GetPrefPath; used by layout persistence)

## 8. Rendering foundation (`editor-viewport`, `ui-render`)

- [ ] 8.1 `[render]` Editor renderer (`engine/sandbox/render` -> `EditorRender`, module scaffold created): record its own pass list on the Aurora device + editor swapchain and present a clear color end to end
- [ ] 8.2 `[render]` GUI pipeline: UI shaders (quad/glyph), PSOs, batching, clip/scissor; consume `sky::ui` draw data and verify a visible single quad
- [ ] 8.3 `[render]` Add the RHI-linked render host target (not the sandbox shell target) and register it in the module deploy deps (`sky_add_dependency(... DEPENDENCIES ...)`)
- [ ] 8.4 `[external]` Engine scene renderer (`aurora-renderer`, top-level Renderer) — needed only for the 3D viewport, not for the editor UI

## 9. Editor viewport (`editor-viewport`)

- [ ] 9.1 `[render]` Create a `ClientViewport` from the main window's native handle and present on resize
- [ ] 9.2 `[render]` Composite the viewport into the main window's swapchain (topology (1)); no child OS window
- [ ] 9.3 `[render]` Editor camera + gizmo/profiler overlays in the same frame
- [ ] 9.4 `[render]` Register the viewport as a layout panel (id in the panel registry/layout)
- [ ] 9.5 `[render]` Viewport selection picking (click/box pick → `SelectionService`)

## 10. Editor shell and panels (`editor-ui-shell`)

- [ ] 10.1 `[render]` Panel hosting on `sky::ui` and panel-instance binding (the layout/registry model already exists in `editor-layout`)
- [ ] 10.2 `[render]` Engine-drawn menu bar/toolbars with `sky::ui`
- [x] 10.3 `[headless]` Input router state: focus, modal depth, and the "UI wants input" gate for the viewport (event dispatch to panels lands with 10.1)
- [ ] 10.4 `[render]` Editor theme/style resource: a default `UIStyle`/theme set applied to shell and panels

## 11. Layout management (`editor-layout`)

- [x] 11.1 Define the toolkit-independent layout model in `EditorCore` (`SplitNode`/`TabNode`/`PanelNode`) and the `PanelRegistry` (id, title, min size, factory)
- [x] 11.2 Layout operations: split, move-to-area, tabify, close, set-ratio, reset-to-default
- [x] 11.3 JSON serialize/deserialize with a version field; skip unknown/missing panel ids (reported as warnings)
- [ ] 11.4 `[render]` `sky::ui` view/interaction only: draggable splitters, tab bar (activate/reorder), panel title bar/close (all model operations are headless)
- [x] 11.5 Headless tests for layout operations and the save/restore round-trip
- [ ] 11.6 `[headless]` (Optional) named workspaces/profiles and floating/detached panels (topology (3))
- [x] 11.7 `[headless]` Layout file persistence: user-config path, restore on startup, save on exit (save-on-change deferred until the panel framework emits layout changes)

## 12. Console & log (`editor-console`)

- [ ] 12.1 `[render]` Implement `IConsoleUI` as a `sky::ui` panel (`UIConsolePanel`): transcript list + command input line (distinct from the ImGui console, which stays runtime/debug only)
- [x] 12.2 `[headless]` Log service: `LogService` installs the logger sink, captures into a bounded ring, and exposes a filtered view (level/tag/search) rebuilt by `Pump()` on the main thread
- [x] 12.3 `[headless]` Command controller: `CommandController` executes via `CommandShell`, records `CommandHistory`, and completes from `CommandRegistry`
- [x] 12.4 `[headless]` Register the "Output Log" and "Console" panel ids (`RegisterDefaultEditorPanels`); `WantsInput` gate provided by `InputRouter` (10.3)
- [x] 12.5 Headless tests: `CommandShell` execute/builtins and `ConsoleLog` ring/pending behavior

## 13. Implement panels (feature parity with legacy where applicable)

- [ ] 13.1 `[render]` World outliner (renders the selection model)
- [ ] 13.2 `[render]` Inspector / property grid (renders the property model)
- [ ] 13.3 `[render]` Asset browser
- [ ] 13.4 `[render]` Asset editors (material / skeleton / animation)

## 14. Cut over / retire legacy

- [ ] 14.1 `[headless]` Make the sandbox editor the shipped editor target (build/packaging decision)
- [ ] 14.2 `[headless]` Decide the fate of the legacy Qt editor `engine/editor` (keep unused vs delete); it is not modified otherwise
- [ ] 14.3 `[headless]` Confirm the new editor never uses the legacy `Renderer::CreateRenderWindow` path and drop lingering references
- [ ] 14.4 `[headless]` Decide whether Qt is removed from the repository entirely

## 15. PIE process isolation

- [ ] 15.1 `[headless]` Add an async process/launch abstraction to the platform layer (spawn with args/cwd, wait, kill, pid, exit status)
- [ ] 15.2 `[headless]` Add an `IpcChannel` (named pipe / unix domain socket) with a versioned length-prefixed protocol
- [ ] 15.3 `[headless]` Define the PIE protocol messages (Ready/Stop/Pause/Resume/Step, Log/Stats/Pick/Error) and a `PlaySession` state machine in `EditorCore`
- [ ] 15.4 `[headless]` Game-side PIE entry: launcher accepts `--pie --ipc --scene`, loads modules/scene, serves IPC, and forwards logs
- [ ] 15.5 `[headless]` Headless PIE mode for CI (no window) using the same protocol
- [ ] 15.6 `[headless]` Crash/timeout handling: IPC drop or process exit sets a crashed state, reaps the process, and surfaces the error
- [ ] 15.7 `[render]` PIE rendering mode 1: separate game window + swapchain
- [ ] 15.8 `[render]` PIE rendering mode 2 (later): shared GPU texture composited into the editor viewport (DX12 shared heap / Metal IOSurface)
- [ ] 15.9 `[render]` Editor play toolbar + session state UI; route forwarded game logs into the Output Log
