## Why

The navigation subsystem (`engine/navigation`, interface module) and its recast backend (`plugins/recast`) are
stale 2024 code. The backend DLL builds but is never loaded at runtime, has no unload path, and contains several
severe correctness/memory defects in the tile-cache build and path query. This change corrects the wiring,
lifecycle, and the severe bugs so navigation is actually loadable and safe.

## What Changes

- **Wiring**: list `RecastNavigation` in the game and editor runtime module configs (`configs/*` and the builtin
  `engine/configs/*`) so `ModuleManager` loads it.
- **Lifecycle**: add `NaviMeshFactory::UnRegister` and make `RecastModule::Shutdown` unregister its factory, so
  unloading the module does not leave a dangling `Impl` pointer; register in `Start()` instead of `Init()` to match
  the physics plugin.
- **Tile build correctness**: fix the tile grid height (`th`) computed from the wrong dimension, and align the
  navmesh `maxTiles` with the tile-cache tile count.
- **Tile data ownership**: remove the double-free of `DT_COMPRESSEDTILE_FREE_DATA` tile data in the nav-data
  destructor and the failure path, and free the `dtTileCache` (no leak / no dangling tile data).
- **Allocation/null safety**: check `dtAllocNavMesh` / `dtAllocNavMeshQuery` / debug technique asset / render
  subsystem results before use.
- **FindPath correctness**: test the end polygon (not the start twice), validate the query filter before use, check
  nav query/status results.
- **Tests**: add a module-load test (mirroring `plugins/audio/test/AudioModuleLoadTest.cpp`) that loads
  `RecastNavigation` through `ModuleManager`, verifies the factory registers, and unloads cleanly.

**Non-goals** (follow-ups): migrating navigation debug draw to aurora, removing the legacy `RenderAdaptor`
dependency, a real `NaviPath` output API and query-filter factory, navigation components/reflection, agent/obstacle
support, automatic world build, and the editor `IWorldBuilderGather` timing fix.

## Capabilities

### New Capabilities
- `navigation-runtime`: runtime module wiring for the recast backend, factory register/unregister lifecycle, and
  safe behavior when no backend is loaded.
- `navigation-recast-backend`: correctness and memory-safety requirements for the recast tile-cache build and path
  query.

## Impact

- Wiring configs: `configs/modules_game.json`, `configs/modules_editor.json`, `engine/configs/modules_game.json`,
  `engine/configs/modules_editor.json`.
- `engine/navigation/include/navigation/NaviMeshFactory.h` + `src/NaviMeshFactory.cpp` (`UnRegister`).
- `plugins/recast/RecastModule.cpp` (register/unregister, phase), `src/RecastnaviMeshGenerator.cpp`,
  `src/RecastTileGenerator.cpp`, `src/RecastNaviMesh.cpp`, `include/recast/*` as needed.
- New test target under `plugins/recast/test`.
- No third-party version changes; `3rdParty::recast` already resolves.
