## 1. Startup modes & console

- [x] 1.1 `SandboxEditor` no `--project` -> Project Manager hub; `--project <path>` -> editor bound to it.
- [x] 1.2 GUI subsystem by default (no console); `--console` attaches a console with the std streams.
- [x] 1.3 No standalone preview window created by the editor (`EditorRenderer::Init(..., withPreview)`).

## 2. Project descriptor & recent list (engine/framework)

- [x] 2.1 `ProjectDescriptor` (`*.skyproj` read/write: id / name / engineVersion / defaultScene / settings / modules / plugins).
- [x] 2.2 `ProjectRegistry` recent list persisted under `GetUserConfigPath()/skyengine/projects.json`.
- [x] 2.3 `ProjectDescriptor::Create` seeds a new project dir (`assets/`, `configs/`, `cache/`) + descriptor.

## 3. Project Manager hub (engine/sandbox)

- [x] 3.1 `ProjectManagerView`: Add / New / Quit + per-selection Open / Remove / Delete + recent rows (select, double-click open).
- [x] 3.2 Add existing via the native file picker; Remove from list; Delete folder (two-step confirm).
- [x] 3.3 Engine-version validation: reject a project newer than the engine, allow older with a warning.

## 4. Single-instance lock & asset mount

- [x] 4.1 `ProjectLock`: `<project>/cache/editor.lock` owner PID; acquire before GPU init; refuse on a live owner; reclaim stale; release on shutdown.
- [x] 4.2 Mount engine + workspace source roots into `AssetDataBase` and `AssetManager::SetWorkFileSystem`.
- [x] 4.3 Deploy engine `assets/` + `configs/` next to the editor.

## 5. Validation

- [x] 5.1 Build `SandboxEditor`.
- [x] 5.2 Hub renders; New creates a project; `--project` opens the editor; a second instance on the same project is refused; no console; no preview window.

## 6. Follow-ups (not in this change)

- [ ] 6.1 Restart / `re-exec` instead of the in-process open.
- [ ] 6.2 Asset browser (`editor-content-browser`), `--safe-mode`, named workspaces, PIE, namespaced plugin mounts.
- [ ] 6.3 Semver version comparison.
