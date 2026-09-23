## Context

- `engine/launcher/windows/Win32Launcher.cpp`: parses `--app`; runs `XRApplication` for `xr`, else
  `GameApplication`. `GameApplication` loads `configs/modules_game.json` and hardcodes `AuroraRender`.
- `engine/sandbox/app`: `EditorApplication` (non-Qt host) creates the native window and loads `SandboxModule`
  (hardcoded); `main.cpp` is the `SandboxEditor` entry and there is a resources deploy step.
- `configs/modules_editor.json`: lists `SandboxModule` (deployed next to the executable by the `Launcher`
  post-build).

The goal is to make the launcher start the editor, reusing the existing editor host logic.

## Goals / Non-Goals

**Goals:**
- `Launcher --app editor` runs the editor.
- The editor's module list comes from `configs/modules_editor.json`.
- No duplication of the editor window/frame-loop logic.
- The editor runtime (module dlls, resources) is available next to the launcher when the sandbox editor is built.

**Non-Goals:**
- Removing the standalone `SandboxEditor` host (kept as a thin convenience entry).
- Changing the editor module, shell, or renderer.

## Decisions

- **D1 - Reuse `EditorApplication` rather than adding a second editor host.** The launcher compiles the sandbox
  `EditorApplication` sources (when `SKY_BUILD_SANDBOX`) and, for `--app editor`, constructs it. Alternative:
  lift the editor host into its own static library — deferred; compiling the small sources into both targets is
  simpler and keeps `SandboxEditor` working unchanged.
- **D2 - Module list from `configs/modules_editor.json`.** `EditorApplication::LoadConfigs` reads the config from
  the bundle path and registers its modules; if the file is missing it falls back to `SandboxModule`. This keeps
  the standalone host working while making the launcher path config-driven.
- **D3 - Gate the editor mode behind the sandbox build.** The editor mode is compiled in only when
  `SKY_BUILD_SANDBOX` is on (the editor host targets exist); otherwise the launcher keeps its current behavior and
  reports that the editor mode is unavailable.
- **D4 - Deploy the editor runtime next to the launcher.** When the sandbox editor is built, add the editor module
  (and its transitive dlls) and the editor `resources/` as launcher deploy dependencies, alongside the existing
  `configs/` deploy.

## Risks / Trade-offs

- [Two ways to start the editor can drift] → both go through the same `EditorApplication` and the same
  `configs/modules_editor.json`.
- [Deploy wiring for the launcher is broader than the editor host] → start with the module + resources; iterate if
  a dll is missing next to the launcher.
- [Launcher links editor code even for game-only users] → gated by `SKY_BUILD_SANDBOX`.

## Migration Plan

1. `EditorApplication::LoadConfigs` reads `configs/modules_editor.json` (fallback `SandboxModule`).
2. Launcher CMake: when `SKY_BUILD_SANDBOX`, compile the editor host sources into `Launcher` and define
   `SKY_EDITOR_HOST`; deploy the editor module + `resources/`.
3. `Win32Launcher`: add `--app editor` (guarded) constructing `sky::editor::sandbox::EditorApplication`.
4. Verify `Launcher --app editor` on Vulkan and DX12; verify `Launcher` (game) and `SandboxEditor` still work.
