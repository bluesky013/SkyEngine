## Why

`plugins/recast` is already render-agnostic: its legacy `RenderAdaptor`/`RenderPrimitive`/`RenderSceneProxy`
integration was removed with the legacy render stack. Navigation's debug view plus runtime wiring now need to target
Aurora.

## What Changes

- Add navmesh debug drawing on Aurora (no legacy path remains).
- Consume the render-agnostic debug geometry already produced by `NaviMesh::BuildDebugGeometry(NaviDebugGeometry &)`
  (added by `fix-navigation-recast`); the core already has no render includes or render link.
- Wire the game-runtime world to create a world, attach `NavigationSystem`, and trigger a nav mesh build.
- Fix the editor `IWorldBuilderGather` timing (the "Build Navigation" action is gathered before modules load).

## Capabilities

### New Capabilities
- `navigation-aurora-integration`: aurora debug rendering for the nav mesh and render-independent navigation
  runtime wiring.

## Related backlog (no change exists yet; create when expanded)

- Navigation path query (not yet a change): a real `NaviPath` output API, a `NaviQueryFilter` creation path,
  and navigation components/reflection so gameplay can request paths.

## Impact

- `plugins/recast` (debug draw, CMake link, includes), `engine/aurora` adaptor/scene (debug draw hook),
  `engine/framework/application` + launcher (world creation and nav build), editor document/menu.
- Navigation backend remains free of render dependencies; only the Aurora debug draw is added.

## Open Questions (expand later)

- Aurora debug-draw API to target (RDG pass vs debug renderer) and whether the nav view lives in an adaptor.
- Who owns the game-world instance and when the nav mesh build runs (world init vs async builder task).
