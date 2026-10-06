## Why

The editor has no project concept: `SandboxEditor` always boots the engine's builtin configuration, so
there is no way to create / open / switch projects, to associate a project with an engine version, or
to isolate per-project state. This change adds the startup / project layer: a Project Manager hub, a
project descriptor, single-instance locking, and project asset mounting.

## What Changes

- `SandboxEditor` with **no `--project`** shows a **Project Manager (hub)**; with **`--project <path>`**
  opens that project in the editor.
- Project descriptor **`*.skyproj`** (id / name / engineVersion / defaultScene / settings / modules /
  plugins) with read/write.
- Hub actions: **Add existing** (browse), **New**, **Open**, **Remove from list**, **Delete folder**
  (two-step confirm), **Quit**; a **recent-project list** persisted under the user config path.
- **Engine-version validation** on open/add (reject a project newer than the engine, allow older with a
  warning).
- **Single-instance hard lock** per project (`<project>/cache/editor.lock`) with stale-lock reclaim.
- **Project work-FS mount** (engine bundle read-only + workspace writable overlay) into
  `AssetDataBase` / `AssetManager`.
- Editor runs as a **GUI-subsystem** app (no console by default; `--console` attaches one) and opens
  **no standalone preview window** (the preview is a docked panel in the target design).
- Deploy engine `assets/` + `configs/` next to the editor so the engine mount and module list resolve.

Not in this change: re-exec (v1 opens in-process), the asset browser, `--safe-mode`, named workspaces,
PIE, and namespaced plugin mounts.

## Capabilities

### New Capabilities

- `editor-project-manager`: editor startup modes, project descriptor, hub, recent list, version
  validation, single-instance lock, and project asset mounting.

### Modified Capabilities

<!-- none -->

## Impact

- `engine/framework`: `framework/project/ProjectDescriptor.{h,cpp}`, `framework/project/ProjectLock.{h,cpp}`.
- `engine/sandbox/app`: `main.cpp` (`--console`), `CMakeLists.txt` (GUI subsystem + assets/configs deploy).
- `engine/sandbox/module`: `SandboxModule.{h,cpp}` (hub/editor branch, lock, mount, hub actions), `CMakeLists.txt` (comdlg32).
- `engine/sandbox/shell`: `ProjectManagerView.{h,cpp}`.
- `engine/sandbox/render`: `EditorRenderer` (`withPreview`).
- `docs/editor/editor-framework-design.md`.
