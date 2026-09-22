## Why

The repository carried two builtin-config sources: the repo-root `configs/` (used by `python/build/Project.py` to
stamp out a game project) and `engine/configs/` (deployed by `Launcher` post-build). They duplicated the same
files (`modules_game.json` / `modules_editor.json` / `asset_build_presets.json` / `image_build_presets.json` /
`render_preload_assets.json`) and had drifted: the root copy still listed the legacy `SkyRender` / `SkyRender.Editor`
module set while `engine/configs/` had moved to the new `SandboxModule` / `AuroraRender` modules. Keeping two
copies means every module-config change has to be applied twice, and a missed copy silently ships stale module
manifests.

## What Changes

- Make the repo-root `configs/` the single source of truth, using the current `engine/configs/` content
  (`modules_editor.json` -> `SandboxModule`; `modules_game.json` -> `AuroraRender` deps; `render_preload_assets.json`
  gains `box`/`draw_id`).
- Delete `engine/configs/` entirely.
- Point the `Launcher` post-build config deployment at `${CMAKE_SOURCE_DIR}/configs` instead of
  `${CMAKE_SOURCE_DIR}/engine/configs`.
- `python/build/Project.py` already reads `<repo>/configs`, so it now copies the authoritative config; no change
  needed there.

## Capabilities

### New Capabilities
<!-- None. -->

### Modified Capabilities
- `aurora-launcher`: the builtin configs now live at the repo-root `configs/` (single directory) instead of
  `engine/configs/`; deployment to the executable's `configs/` is unchanged.

## Impact

- `configs/` (now the authoritative builtin set; `default_console.cfg` retained).
- `engine/configs/` removed.
- `engine/launcher/CMakeLists.txt` (post-build source path).
- `engine/framework/src/application/GameApplication.cpp` (comment).
- `engine/aurora/AGENTS.md`, `openspec/specs/aurora-launcher/spec.md` (docs).
