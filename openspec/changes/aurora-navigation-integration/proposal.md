## Why

`plugins/recast` still integrates with the legacy render path: it links `RenderAdaptor`, builds a legacy
`RenderPrimitive`, and attaches through `RenderSceneProxy`. The engine is migrating to aurora, and navigation's
debug view plus runtime wiring must move there too. This is deferred so `fix-navigation-recast` can correct the
core first, and the recast runtime wiring is intentionally render-agnostic until then.

## What Changes

- Migrate navmesh debug drawing from the legacy `RenderPrimitive`/`RenderSceneProxy` path to aurora.
- Consume the render-agnostic debug geometry already produced by `NaviMesh::BuildDebugGeometry(NaviDebugGeometry &)`
  (added by `fix-navigation-recast`); the core is already free of render includes and the `RenderAdaptor` link.
- Wire the game-runtime world to create a world, attach `NavigationSystem`, and trigger a nav mesh build.
- Fix the editor `IWorldBuilderGather` timing (the "Build Navigation" action is gathered before modules load).

## Capabilities

### New Capabilities
- `navigation-aurora-integration`: aurora debug rendering for the nav mesh and render-independent navigation
  runtime wiring.

## Related backlog (to split into its own change when expanded)

- `navigation-path-query`: a real `NaviPath` output API, a `NaviQueryFilter` creation path, and navigation
  components/reflection so gameplay can request paths.

## Impact

- `plugins/recast` (debug draw, CMake link, includes), `engine/aurora` adaptor/scene (debug draw hook),
  `engine/framework/application` + launcher (world creation and nav build), editor document/menu.
- Removes the legacy `RenderAdaptor` dependency from the navigation backend.

## Open Questions (expand later)

- Aurora debug-draw API to target (RDG pass vs debug renderer) and whether the nav view lives in an adaptor.
- Who owns the game-world instance and when the nav mesh build runs (world init vs async builder task).
