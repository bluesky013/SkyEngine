## 1. Runtime wiring

- [x] 1.1 Add `RecastNavigation` to `configs/modules_game.json` and `engine/configs/modules_game.json`
- [x] 1.2 Add `RecastNavigation` to `configs/modules_editor.json` and `engine/configs/modules_editor.json`

## 2. Lifecycle

- [x] 2.1 Add `NaviMeshFactory::UnRegister()` (clears the registered impl)
- [x] 2.2 `RecastModule` registers in `Start()` and unregisters in `Shutdown()`; fix the namespace comment

## 3. Tile build correctness

- [x] 3.1 Compute tile-grid height from `gh` in `PrepareTiles`
- [x] 3.2 Set `RecastNaviMapConfig::maxTiles` default to `RECAST_MAX_BUILD_TILES`
- [x] 3.3 Fix tile-data ownership: detach on successful `addTile`, free once on failure
- [x] 3.4 Own and free `dtTileCache` in the generator (destructor + init failure path)

## 4. Null/allocation safety

- [x] 4.1 Check `dtAllocNavMesh` / `dtAllocNavMeshQuery` before use in `RecastNaviMesh`
- [x] 4.2 Fail `BuildNavMesh` cleanly when nav mesh init fails; free the query when init fails

## 5. Path query correctness

- [x] 5.1 Test `endPoly` instead of `startPoly` twice
- [x] 5.2 Require a non-null filter and nav query; treat zero/failed results as failure

## 6. Render decoupling

- [x] 6.1 Add render-agnostic `NaviDebugGeometry` and `NaviMesh::BuildDebugGeometry`
- [x] 6.2 Remove render includes/members/technique from `RecastNaviMesh`; port debug draw to geometry data
- [x] 6.3 Stop the build pipeline from calling render draw; drop `RenderAdaptor` from the plugin link

## 7. Tests and verification

- [x] 7.1 Add `plugins/recast/test` module-load test (load, assert factory registered, unload, assert cleared)
- [x] 7.2 Build `Navigation`, `RecastNavigation`, and `RecastModuleTest`
- [x] 7.3 Run the test and `openspec validate fix-navigation-recast --strict`
