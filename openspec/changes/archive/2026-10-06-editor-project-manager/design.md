## Context

Full design: `docs/editor/editor-framework-design.md`. This change implements the first vertical slice
(P0) of that design. The editor is the non-Qt sandbox (`engine/sandbox`), and its base modules come
from the engine bundle, so the hub can boot with no project.

## Goals / Non-Goals

**Goals**

- Project startup: hub (no project) vs editor (with a project).
- `.skyproj` descriptor + recent list + version validation.
- Single editor instance per project; project asset mount.

**Non-Goals (this change)**

- Restart/`re-exec` (v1 switches **in-process** from hub to the editor shell).
- Asset browser, `--safe-mode`, named workspaces, PIE, namespaced plugin mounts.
- Overlapping-dock / floating windows.

## Decisions

- **Descriptor in `engine/framework`.** `ProjectDescriptor` (data) + `ProjectRegistry` (recent list)
  live in the framework (consumer side, `AGENTS.md`); the hub UI / actions live in `engine/sandbox`.
- **Same executable, two modes.** `SandboxEditor` decides by `--project`; the hub is a
  `sky::ui` element (`ProjectManagerView`), not OS widgets. The file-picker for "add existing" is the
  one native dialog (allowed).
- **Hard lock before GPU work.** `ProjectLock` acquires `<project>/cache/editor.lock` *before*
  `EditorRenderer::Init`; a live owner PID refuses the second instance (stale locks reclaim). Because
  `ModuleManager` ignores a failed module `Init`, refusal also calls `ISystemNotify::SetExit()`.
- **Mount mirrors the legacy editor.** `AssetDataBase::SetEngineFs(<bundle>)` +
  `SetWorkSpaceFs(<project>/assets)` + `AssetManager::SetWorkFileSystem(<project>)`; paths are relative
  to each side's `assets/` root, workspace shadows engine.
- **GUI subsystem.** Build `SandboxEditor` with `WIN32_EXECUTABLE` + `/ENTRY:mainCRTStartup` so no
  console opens; `--console` calls `AllocConsole()` + reopens the std streams.
- **No standalone preview window.** The preview is a docked panel in the design; until docking exists,
  `EditorRenderer::Init(..., withPreview=false)` creates no separate window.

## Risks / Trade-offs

- [In-process open vs re-exec] -> documented v1 simplification; re-exec is a follow-up.
- [Lexicographic version compare] -> acceptable for `x.y.z`; a semver resolver is a follow-up.
- [Native file dialog blocks the frame loop] -> acceptable while modal; a `sky::ui` browser is future.
- [Raw `new NativeFileSystem` in the mount] -> matches the legacy editor; `CounterPtr` adopts it.

## Migration Plan

Additive. Rollback is reverting the touched files; a stale `editor.lock` is reclaimed automatically.
