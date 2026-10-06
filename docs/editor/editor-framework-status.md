---
title: "Editor Framework - Status & Handoff"
description: "Operational state of the sandbox editor work, for resuming in a new session."
updated: "2026-10-07"
---

## Where things stand

- **Branch**: `dev_refactor_rhi` (ahead of `origin` by 3 commits).
- **Landed (committed)**:
  - `[fix]: correct astcenc_context_alloc arity …` (839960d8)
  - `[tool]: add third-party version precheck (--check/--strict)` (8dc58e0e)
  - `[feat]: engine file browser dialog …` (21c1f5f8)
  - earlier: editor project-manager hub (0281296f), interactive docking/floating (0fe0187f),
    SDL removal / Win32-only window backend (608e420d).
- **Uncommitted (working tree)**: `editor-preferences-dialog` and `editor-window-state` code + their
  openspec artifacts, the shared `ModalDialog` / dialog-skin refactor, and a `FileIO::WriteString` fix.

Design: `docs/editor/editor-framework-design.md`. Changes: `openspec/changes/editor-*/`.

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

- Commit / archive the completed `editor-file-browser-dialog` / `editor-window-state` and finish
  `editor-preferences-dialog` (tasks 3.3, 4.3, 4.5, 5.1).
- `editor-project-manager` follow-ups (§6): `re-exec`, hub as a separate top-level window.
- Asset browser (`editor-content-browser`) reusing `IFileBrowserSource`/`FileBrowserModel`.
