---
title: "Editor Framework - Status & Handoff"
description: "Operational state of the sandbox editor / project-manager work, for resuming in a new session."
updated: "2026-10-06"
---

## Where things stand

- **Branch**: `dev_refactor_rhi` (ahead of `origin` by 4 commits).
- **Recent commits**: `[fix]: event-driven window close …`, `[feat]: editor UI icon pipeline …`,
  `[feat]: ui anti-aliasing … and dpi scaling`, `[feat]: editor property panels …`,
  `[feat]: sandbox reflection widget editor framework`.
- **Uncommitted (working tree)**: the `editor-project-manager` slice — code + `docs/editor/editor-framework-design.md`
  + `openspec/changes/editor-project-manager/`. **Nothing committed for it yet**, not pushed.

Design: `docs/editor/editor-framework-design.md`. Change: `openspec/changes/editor-project-manager/`.

## Implemented (uncommitted) — editor startup / project manager

- `SandboxEditor` **no `--project`** → Project Manager **hub**; **`--project <path>`** → editor bound to it.
- **GUI subsystem**: no console by default; **`--console`** attaches one (stdout/stderr).
- **Hub** (`ProjectManagerView`): **Add existing** (native file picker + `engineVersion` validation),
  **New**, **Open**, **Remove from list**, **Delete folder** (two-step confirm), **Quit**; recent rows
  select + double-click open.
- **`*.skyproj`** descriptor read/write; **recent list** persisted at
  `GetUserConfigPath()/skyengine/projects.json`.
- **Single-instance hard lock**: `<project>/cache/editor.lock` (owner PID, stale reclaim); a second
  editor on the same project is refused.
- **Project work-FS mount**: `AssetDataBase` engine (read-only) + workspace (writable) +
  `AssetManager::SetWorkFileSystem`.
- Engine **`assets/` + `configs/`** deployed next to the editor.
- Editor opens **no standalone preview window** (`EditorRenderer::Init(..., withPreview=false)`).

Files:

- framework: `engine/framework/{include,src}/project/{ProjectDescriptor,ProjectLock}.*`
- sandbox/app: `engine/sandbox/app/{src/main.cpp,CMakeLists.txt}` (GUI subsystem, assets/configs deploy)
- sandbox/module: `engine/sandbox/module/{include,src}/…/SandboxModule.*`, `CMakeLists.txt` (comdlg32)
- sandbox/shell: `engine/sandbox/shell/{include,src}/…/ProjectManagerView.*`
- sandbox/render: `engine/sandbox/render/{include,src}/…/EditorRenderer.*` (`withPreview`)
- docs: `docs/editor/editor-framework-design.md`, `docs/README.md`

## Build / run

- Close any running `SandboxEditor.exe` first (it locks `SandboxModule.dll`/`dxcompiler.dll`).
- Reconfigure (new files use `GLOB`): `cmake -S . -B build`
- Build: `cmake --build build --config Release --target SandboxEditor`
- Run hub: `output/bin/Release/SandboxEditor.exe`
- Run a project: `output/bin/Release/SandboxEditor.exe --project <path>/<name>.skyproj`
- Live logs: append `--console`.

## Verified

Build green; hub renders; **New** creates a real project (`.skyproj` + `assets/configs/cache`) and the
recent list updates; `--project` opens the editor; a **second** instance on the same project is
**refused**; no console window; no preview window.

## Interactive docking + floating + SDL removal (uncommitted)

Change: `openspec/changes/editor-interactive-docking/` (separate from `editor-project-manager`).

- **Docking**: engine menu bar (File/Edit/View/Window/Tools/Help) + status bar replace the flat toolbar;
  view registry survives rebuilds; draggable splitters (hover grab band + grip + cursor change);
  tab drag/reorder/tabify + drop-zone highlight + drag ghost; tab close; `View > Reset Layout`.
- **Persistence**: per-user `GetUserConfigPath()/editor_layout.json` (v2 with a `floating` array),
  auto-saved coalesced at end-of-frame and restored on startup.
- **Floating (tear-out)**: dragging a tab out of the dock area floats it into its own OS window;
  per-surface `UIContext`/`UIRenderer` on the one device; shared font atlas across windows;
  close → re-dock; geometry write-back; startup restore; per-window DPI. Win32-only.
- **Style**: UE + Blender hybrid — rounded panels with a 3px gap, flat muted headers, subtle separators,
  balanced default ratios (Outliner 22% / center ~53% / Inspector 25%).
- **SDL removed project-wide**: deleted `3rdParty::sdl` (`cmake/thirdparty.json`/`.cmake`, `Findsdl.cmake`)
  and the SDL/macOS window backend (`platform/genetic/SDL*`, `platform/macos/Macos*`).
  **macOS is now unsupported** until a native backend is added; Windows is the only backend.
- Core helpers added: `core/layout/DockInteraction` (geometry) and `core/shell/ShellModels` (view-model).

## Gotchas

- Two **stuck zombie** `SandboxEditor.exe` (0 MB, no owner/path) cannot be killed without admin:
  `taskkill /F /IM SandboxEditor.exe` from an **elevated** prompt (or reboot).
- Capturing the window from a **non-DPI-aware** PowerShell yields virtualized coordinates; call
  `SetProcessDPIAware()` and use `PrintWindow`. Injected mouse messages are DPI-scaled (×1.25 here),
  so synthetic click coordinates must be divided by the scale — real input is unaffected.
- `engineVersion` compare is lexicographic (`x.y.z`); replace with semver later.
- Opening a project is **in-process** (v1), not the designed `re-exec`.

## Next steps

See `openspec/changes/editor-project-manager/tasks.md` §6:

- `re-exec` instead of the in-process open; hub as a **separate top-level window**.
- Asset browser (`editor-content-browser`), `--safe-mode`, named workspaces, PIE, namespaced plugin mounts.
- Commit / archive `editor-project-manager` (needs explicit user confirmation per `AGENTS.md`).
