---
title: "Editor Framework - Status & Handoff"
description: "Operational state of the sandbox editor work, for resuming in a new session."
updated: "2026-10-07"
---

## Where things stand

- **Branch**: `dev_refactor_rhi` (in sync with `origin`).
- **Landed (committed)**:
  - `[fix]: correct astcenc_context_alloc arity …` (839960d8)
  - `[tool]: add third-party version precheck (--check/--strict)` (8dc58e0e)
  - `[feat]: engine file browser dialog …` (21c1f5f8)
  - `[feat]: editor preferences dialog, window-state persistence, shared dialog refactor` (3920c18c)
  - `[doc]: editor preferences docs; archive editor-preferences-dialog` (d76fd940)
  - earlier: editor project-manager hub (0281296f), interactive docking/floating (0fe0187f),
    SDL removal / Win32-only window backend (608e420d).
- **This change set (being committed, both now archived)**: `world-subsystem-registry` — the registry +
  `WorldDesc` + `World::Build` (framework), `WorldDocument` + `WorldConfigPanel` + New/Open/Save/Close World
  dialogs + `Ctrl+S`/`Ctrl+W` + title/status (sandbox), physics/nav subsystem registration (plugins). Plus
  `editor-play-in-editor` (PIE): world duplication + `PlaySession` + Play menu / `F5`/`Shift+F5`.
  Archived to `openspec/changes/archive/2026-10-07-{world-subsystem-registry,editor-play-in-editor}`; main
  specs synced to `openspec/specs/{world-subsystem-registry,editor-play-in-editor}`.

Design: `docs/editor/editor-framework-design.md`. World subsystems: `docs/features/world-subsystems.md`.
PIE: `docs/editor/play-in-editor.md`. Changes: `openspec/changes/editor-*/`.

## Editor dialogs (shell)

- **File browser dialog** (`editor-file-browser-dialog`): engine-drawn modal over the hub / editor; a headless
  `FileBrowserModel` + `IFileBrowserSource` (`FileSystemSource`); `OpenFile` / `OpenProject` /
  `SelectDirectory` modes; extension + asset-type filters; Places sidebar; New Folder (write mode); context
  menu + inline rename. Project Manager **New** → directory chooser, **Add** → `*.skyproj` chooser (retired
  the raw Win32 open dialog).
- **Preferences dialog** (`editor-preferences-dialog`): registry-driven pages (General/Editor/Rendering)
  editing a `PreferenceStore`; OK/Apply/Cancel/Reset; persists to `editor-preferences.json`. Opened from
  **File > Preferences…**; the old docked `config` panel was removed.
- Both derive from a shared **`ModalDialog`** base (backdrop / centered panel / open state) and paint via
  **`UiSkin`/`UiTheme`**, matching the reflected panels. Shell routes input to the topmost modal via
  `EditorShell::ActiveModal()`.

## Window state (`editor-window-state`)

- Main window size/position persisted to `<user-config>/skyengine/editor_window.json` (restored on launch,
  saved on graceful exit; `--frames` dev runs skip saving).
- Backend support: `Win32Window` reports live client size on `WM_SIZE`; `NativeWindow::GetPosition/
  SetPosition` (Win32 `GetWindowRect`/`SetWindowPos`).

## DPI / UI scale

Design: `docs/editor/ui-sizing-and-scaling.md` (single style source, ImGui-style).

- Windows Per-Monitor-V2 aware; `Win32Window::GetDpiScale()` = `GetDpiForWindow / 96`. `SandboxModule`
  computes `systemUiScale` (overridable by `SKY_UI_SCALE`) and the **effective** scale =
  `systemUiScale * editor.uiScale` (preference, 0.5–2.0, default 1.0).
- `UiTheme` is the single source: `MakeDarkTheme(scale)` → `UiMetrics::Scale(scale)` (analogue of
  `ImGuiStyle::ScaleAllSizes`) + scaled `UiFonts`. All views read `metrics` / `fonts` — no hard-coded
  layout pixels, no per-view scale shims.
- Applied to **both** the editor (`EditorShell::SetUiScale`) and the Project Manager hub
  (`SetDefaultUiTheme` before building the hub view — previously the hub was unscaled). Applied at startup
  (restart to change).

## World subsystems (`world-subsystem-registry`, uncommitted)

- Framework: `WorldSubSystemRegistry` (a cross-DLL `Singleton<T>`) + `WorldDesc` +
  `World::Build`/`GetWorldDesc`; `World::SaveJson/LoadJson` persist `subSystems[{name,enabled,config}]`.
  See `docs/features/world-subsystems.md`.
- Sandbox: `WorldDocument` owns a `sky::World` and `Load`/`Save`s a `.world`; `WorldConfigPanel`
  (tabbed with the Outliner) lists registered subsystems with an **enable checkbox** (toggle → `Rebuild` +
  `Save`) and hosts a `ReflectedFormView` bound to the selected subsystem's config. **File** = New World… /
  Open World… / Save World (`Ctrl+S`) / Close World (`Ctrl+W`) / Quit (saves a dirty world first).
- Plugins: `plugins/bullet` registers `"Physics"` (`PhysicsSubSystemConfig`), `plugins/recast`
  registers `"Navigation"` (`NavigationSubSystemConfig`); both added to `configs/modules_editor.json`.
- Tests: `FrameworkTest.WorldSubSystemRegistryTest`, `EditorCoreTest.WorldDocumentTest`,
  `EditorShellTest` (world-config toggle) — all green.

## Play-In-Editor (`editor-play-in-editor`)

- `WorldDocument::CreatePlayWorld()` duplicates the edit world by serializing it and loading into a fresh world
  (actors/components + `WorldDesc`), then `Build` + `StartSimulation`. `World` gained
  `StartSimulation`/`StopSimulation`.
- `editor::PlaySession` (in `editor/core`) owns the runtime world with `Editing`/`Playing`/`Paused`; the host
  `SandboxModule::Tick` drives it. **Play** menu + `F5` (toggle Play/Pause) / `Shift+F5` (Stop); the status bar
  mode shows `Edit`/`Play`/`Pause`. Opening/creating a world stops the session first.
- Non-goal: rendering the play world (needs the render loop) — `PlaySession::GetWorld()` is the seam.
- Tests: `FrameworkTest.WorldSubSystemRegistryTest.StartStopSimulationIteratesSubsystems`,
  `EditorCoreTest.PlaySessionTest.*` + `WorldDocumentTest.CreatePlayWorldDuplicates`,
  `EditorShellTest.F5TogglesPlayPauseAndShiftStops`.

## Document title / status (`Ctrl+S`, dirty marker)

- The open world name shows in the **OS window title** and at the **front of the status bar**, with a `*`
  while unsaved: `FormatWindowTitle` / `FormatStatusBar` (`editor/core/shell/ShellModels`). Host wiring:
  `SandboxModule::RefreshDocumentInfo` (on open/new/save + each tick) → `EditorShell::SetDocumentInfo` →
  `titleHandler` sets `NativeWindow::SetTitle` (`NativeWindowManager::GetMainWindow`).
- **Ctrl+S** saves the world and **Ctrl+W** closes it (File > Save World / Close World; Close saves if
  dirty then unloads, returning to `Untitled`); config edits mark the document dirty
  (`ReflectedForm::SetOnChanged` → `WorldDocument::MarkDirty`) so the marker is accurate.
- The Config panel shows **"No world open"** before a world is opened (never fabricated defaults).
- **Reset baseline = the type's default value** for every reflected form (Inspector + Config):
  `ReflectedForm` captures defaults from a default-constructed instance (`MakeDefaultValue`), so reset means
  "restore the type default" (UE/Godot-style) and persists across load/save. The ad-hoc per-panel baseline
  plumbing was removed.
- Cleanup: removed dead `ReflectedConfigPanel` + `IEditorConfigSource`/`NamedPropertyObject` and the unused
  `World::worldConfigs` / `RegisterConfiguration` / `GetConfigByName` / `GetMutableConfigByName`.

## Post-mortem: world-config / reset

Three distinct defects surfaced while wiring the world-config surface, in report order:

1. **Config fabricated defaults with no document open.** `WorldConfigPanel::Rebind()` fell back to
   `reg->makeDefaultConfig()` into a local `fallbackConfig` when `documentProvider()` was null, so the panel
   showed editable values unrelated to any world. Fix: an empty **"No world open"** state; never fabricate.
2. **Reset target drifted with the session.** `ReflectedForm` captured its "default" by snapshotting the live
   values on first bind, so after load the baseline equalled the loaded value → `IsModified` false, no reset
   icon, and reset a no-op. Fix: reset baseline = the type's **default-constructed value**
   (`MakeDefaultValue` / `TypeInfoRT::newFunc`), used uniformly by Inspector + Config.
3. **The reset icon click did nothing (the actual "no effect").** `ReflectedFormView::OnPointerEvent` forwarded
   every row click to the leaf widget, and `GenericReflectedWidget::OnDown` returns `true` unconditionally
   (bool toggles, float starts an edit/drag). The revert-icon click was consumed before `BeginInteraction()`
   (the reset handler) ran. Fix: dispatch to a widget only when the click is inside `controlRect`; label /
   revert clicks fall through to `BeginInteraction`.

Why it slipped: the model was correct, so `ReflectedForm`-level tests passed while the UI path was broken; no
test drove a pointer event to the revert icon; the two earlier fixes addressed real but *different* problems,
masking the routing bug. Guardrails: `EditorShellTest.RevertIconClickResetsField` (fails without the hit-area
fix), `ReflectedFormTest` (reset-to-default for float/int/bool + rebind), and documented hit-area/reset rules in
`docs/editor/reflection-widget-framework.md`.

## Build / run

- **Close any running `SandboxEditor.exe` first** — it locks `SandboxModule.dll`/`dxcompiler.dll`, so the
  shader target's post-build copy fails and the app target never relinks.
- Editor UI lives in **`SandboxModule.dll`** (loaded at runtime): after changing shell/module code, build the
  **`SandboxModule`** target, not only `SandboxEditor`.
- New files use `GLOB`: re-run `cmake -S . -B cmake-build-debug` before building them.
- Run hub: `output/bin/Debug/SandboxEditor.exe`; run a project: add `--project <path>.skyproj`; logs: `--console`.

## Gotchas

- The editor auto-formats on file write (clang-format): edits can reflow whole files / move brace blocks;
  prefer small, exact edits.
- `ScanCode` vs virtual-key: the platform forwards the engine `ScanCode` enum; `SandboxModule` maps once to
  VK (and drops `WM_CHAR` control codes) so all UI widgets agree.
- `engineVersion` compare is lexicographic (`x.y.z`); replace with semver later.
- Opening a project is **in-process** (v1), not the designed `re-exec`.

## Next steps

- Verify + commit `world-subsystem-registry` (registry + editor Config panel); open tasks: 3.3
  (manual verify) and the deferred config→runtime mapping (physics still via `SetWorldAttacher`).
- Verify + commit `editor-play-in-editor` (PIE); open task: 4.2 (manual verify). Rendering the play world and
  game input are deferred to the render path.
- `editor-project-manager` follow-ups (§6): `re-exec`, hub as a separate top-level window.
- Asset browser (`editor-content-browser`) reusing `IFileBrowserSource`/`FileBrowserModel`.
- Render-scene bridge subsystem (`IRenderSceneSubSystem` + aurora adaptor) — with the render main loop.
