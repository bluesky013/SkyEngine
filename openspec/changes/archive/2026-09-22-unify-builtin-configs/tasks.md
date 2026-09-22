## 1. Single builtin config source

- [x] 1.1 Overwrite repo-root `configs/` with the current `engine/configs/` content (modules_game/modules_editor/asset_build_presets/image_build_presets/render_preload_assets)
- [x] 1.2 Keep `configs/default_console.cfg` (present only in the root copy)
- [x] 1.3 Delete `engine/configs/`

## 2. Deployment

- [x] 2.1 Point `Launcher` post-build deployment at `${CMAKE_SOURCE_DIR}/configs`
- [x] 2.2 Confirm `python/build/Project.py` reads `<repo>/configs` (no change needed)

## 3. Docs

- [x] 3.1 Update the `GameApplication` builtin-config comment
- [x] 3.2 Update `openspec/specs/aurora-launcher/spec.md` and `engine/aurora/AGENTS.md` to reference `configs/`
