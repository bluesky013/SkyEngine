## Why

The editor is currently started by a dedicated executable, `SandboxEditor` (`engine/sandbox/app`), which is a
second host alongside the game launcher (`engine/launcher` -> `Launcher`). The intended model is a single launcher
binary that starts the editor when asked to, so the editor is one application mode rather than a separate host
with its own deployment.

Concretely today: `Win32Launcher` only selects `GameApplication` (game) or `XRApplication` (`--app xr`);
`GameApplication` loads `configs/modules_game.json`. Nothing starts the editor, and `configs/modules_editor.json`
is unused by the new architecture.

## What Changes

- Add an **editor application mode to the launcher**: `Launcher --app editor` runs the editor host.
- The editor mode **loads modules from `configs/modules_editor.json`** (deployed next to the launcher) instead of
  hardcoding the module list, falling back to `SandboxModule` when the config is absent.
- **Reuse the sandbox editor host** (`sky::editor::sandbox::EditorApplication`) instead of duplicating window/loop
  logic; `SandboxEditor` remains as a thin standalone entry for convenience.
- **Deploy the editor runtime next to the launcher**: the editor module and its resources when
  `SKY_BUILD_SANDBOX` is on (module dlls + `resources/`), in addition to the already-deployed `configs/`.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
- `aurora-launcher`: the launcher supports an editor application mode (`--app editor`) that runs the editor host.
- `editor-application`: the editor may be launched either by the dedicated `SandboxEditor` host or by the launcher's
  editor mode; module loading follows `configs/modules_editor.json` when present.

## Impact

- `engine/launcher/windows/Win32Launcher.cpp` (editor mode), `engine/launcher/CMakeLists.txt` (link the editor host,
  deploy editor runtime).
- `engine/sandbox/app/src/EditorApplication.cpp` (load `configs/modules_editor.json`, fall back to `SandboxModule`).
- `configs/modules_editor.json` (already lists `SandboxModule`).
- Not changed: the editor module (`SandboxModule`), the shell, and the editor renderer.
