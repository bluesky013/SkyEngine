## Context

`engine/navigation` is a STATIC interface module (`NaviMeshFactory` singleton + `Impl` factory, `NavigationSystem`
`IWorldSubSystem`) mirroring `engine/physics`. `plugins/recast` implements the factory and builds a Detour
tile-cache navmesh from collision triangle meshes.

Two problems: (1) the plugin is never listed in any runtime module config, so `ModuleManager` never loads it, and
there is no unregister path; (2) the tile-cache build and query contain severe correctness/memory defects. This
change fixes wiring, lifecycle, and those defects, keeping the legacy render/debug-draw path untouched.

## Goals / Non-Goals

**Goals**
- `RecastNavigation` loads at runtime in game and editor configs like other backend plugins.
- Factory register/unregister lifecycle is symmetric; unloading the module leaves no dangling pointer.
- The tile-cache build is correct and leak/double-free free; allocations are checked; `FindPath` is defensively
  correct.
- A module-load test verifies registration without a renderer.

**Non-Goals**
- Aurora debug-draw migration (consuming the render-agnostic `NaviDebugGeometry`), a real `NaviPath` output API,
  query-filter factory, navigation components/reflection, agents/obstacles, automatic world build, and the editor
  `IWorldBuilderGather` timing fix. These stay as follow-ups; the legacy `RenderAdaptor` dependency is removed
  here (D10).

## Decisions

### D1. Runtime wiring via module configs (render-agnostic)
Add `RecastNavigation` with **empty dependencies** to `configs/modules_game.json`, `configs/modules_editor.json`,
`engine/configs/modules_game.json`, `engine/configs/modules_editor.json`. It is deliberately not wired to a
specific renderer (neither legacy `SkyRender` nor `AuroraRender`): the core build/query and the (currently
non-functional) debug attachment are guarded and must not force a render module. The aurora debug-view integration
is tracked by the `aurora-navigation-integration` backlog change. The build switch stays `SKY_BUILD_RECAST`;
register/unregister makes load order irrelevant.

### D2. Symmetric factory lifecycle
Add `NaviMeshFactory::UnRegister()` (resets `factory`), mirroring `PhysicsRegistry::UnRegister`. `RecastModule`
registers in `Start()` and unregisters in `Shutdown()`, matching `BulletPhysicsModule` (not `Init`), so the
`Impl` never outlives its DLL.

### D3. Tile data ownership
`dtTileCache::addTile(..., DT_COMPRESSEDTILE_FREE_DATA, ...)` transfers ownership on success. After a successful
`addTile`, detach `navData->data` (set to `nullptr`) before releasing the `RecastNavData`, so its destructor does
not double-free. On failure, ownership stays with us: free once and detach, then release.

### D4. Tile cache lifetime
`RecastNaviMeshGenerator` owns `dtTileCache*` and frees it with `dtFreeTileCache` in its destructor; the failure
path in `PrepareTileCache` frees it before returning. This removes the leak and the dangling tile data.

### D5. maxTiles consistency
`RecastNaviMapConfig::maxTiles` default becomes `RECAST_MAX_BUILD_TILES` (the same constant the tile cache uses),
so every tile the cache accepts fits the navmesh.

### D6. Tile grid dimension
`PrepareTiles` computes `th` from `gh` (the second `rcCalcGridSize` output), not `gw`.

### D7. Allocation and null safety
Check `dtAllocNavMesh` / `dtAllocNavMeshQuery` before use, check the debug technique asset in the `RecastNaviMesh`
constructor, and null-check the render-scene subsystem in `OnAttachToWorld` / `OnDetachFromWorld`.

### D8. FindPath defensive correctness
Test `endPoly` (not `startPoly` again), require a non-null filter and non-null `navQuery`, and treat zero/failed
results as `FAILED`. Returning an actual `NaviPath` stays out of scope (no `NaviPath` output API yet).

### D10. Render decoupling

The debug visualization is removed from the core. `NaviMesh` gains a render-agnostic
`BuildDebugGeometry(NaviDebugGeometry &)` that returns a triangle list of positions/colors, and the recast backend
no longer includes any render header, holds a `RenderPrimitive`, loads a technique asset, or links `RenderAdaptor`.
The build pipeline no longer calls a render draw. A renderer consumes the geometry later (tracked by the
`aurora-navigation-integration` backlog change).

### D9. Verification
Add `plugins/recast/test` using `ModuleManager` to load `RecastNavigation`, assert `NaviMeshFactory::CreateNaviMesh`
becomes non-null, then unload and assert it is null again (mirrors `plugins/audio/test/AudioModuleLoadTest.cpp`).

## Risks / Trade-offs

- [Registering in `Start` instead of `Init` changes when the factory is available] -> World building happens after
  module start; gameplay does not query navigation during `Init`.
- [Adding `RecastNavigation` to game configs activates legacy debug-draw code paths] -> Debug draw is already
  non-functional but guarded; `OnAttachToWorld` null checks prevent crashes when no render subsystem exists.
- [`FindPath` still cannot return points] -> explicitly out of scope; the fix only removes the correctness bugs and
  unsafe casts so it is safe to call.
- [Tile ownership detach on success relies on Detour semantics] -> documented behavior of
  `DT_COMPRESSEDTILE_FREE_DATA`; the test exercises load/unload only, build is covered by code review.

## Migration Plan

1. Add `NaviMeshFactory::UnRegister` + `RecastModule` register/unregister.
2. Fix tile ownership/leak/maxTiles/grid in the generator and navmesh config.
3. Add null/allocation checks and the `FindPath` corrections.
4. Add module configs and the module-load test.
5. Build `Navigation`, `RecastNavigation`, and the test; run the test.

Rollback: remove `RecastNavigation` from the module configs; the interface module and null-backend path are
unaffected.

## Open Questions

- Should the debug-draw legacy dependency be removed next, or migrated to aurora?
- Where should navmesh build be triggered in the game runtime (world init vs an explicit builder call)?
