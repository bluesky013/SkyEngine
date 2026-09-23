## Context

The terrain→navigation core is implemented and unit-tested: `TerrainGeometryProvider` emits world-space LOD0 triangles; `TerrainNavRebuildCoordinator` maps terrain changes to nav tiles, defers on missing LOD0, and drives `SetRebuildTiles`; `TerrainNavigationBridge` registers the provider with a world's `NavigationSystem`. The recast backend gathers geometry from registered providers (plus collision components) into an octree and rasterizes tiles.

Missing is end-to-end validation: building an actual nav mesh through recast from terrain and checking walkability, and building from terrain + collision together. This needs a World with a `NavigationSystem`, a terrain system/provider, and the recast factory registered, plus enough of the build pipeline to run a generator and rasterize tiles.

## Goals / Non-Goals

**Goals:**

- Provide a reusable World-level harness to run a nav mesh build with configurable geometry sources.
- Validate terrain LOD0 → nav mesh walkability over flat and sloped terrain.
- Validate terrain provider + collision geometry combined into one build.

**Non-Goals:**

- Changing nav/terrain behavior or the provider seam.
- Path query quality/tuning beyond a basic walkability assertion.
- Editor-level workflows.

## Decisions

### D1: World-level harness in the recast/navigation test side

**Decision**: Build the harness where the recast backend and navigation core meet (a test target that can register the recast factory and create a World with `NavigationSystem` + a terrain provider), rather than in the engine core (which has no backend).

**Rationale**: End-to-end nav mesh construction inherently requires a backend; the engine core stays backend-free and keeps its unit tests.

**Alternatives considered**: Mock the backend (does not validate the recast path). Add engine-core dependencies on recast (violates layering).

### D2: Assert on walkable area / reachability, not exact geometry

**Decision**: Assert quantitative properties (walkable area presence/coverage, a reachable path across the region) over flat and sloped terrain, and that combined sources both contribute (e.g., geometry from both is present/reflected in the mesh).

**Rationale**: Exact navmesh output is backend-version-sensitive; property assertions are stable and meaningful.

**Alternatives considered**: Golden mesh comparison (brittle).

## Risks / Trade-offs

- [World/harness setup cost] -> Keep the harness minimal (World + NavigationSystem + provider + factory); reuse the recast test registration.
- [Backend/platform variability] -> Use property assertions with tolerances.
- [Test runtime] -> Keep build regions small (a few nav tiles).

## Migration Plan

1. Add the harness (World + NavigationSystem + recast factory + a way to inject geometry sources).
2. Add the flat/sloped walkability test.
3. Add the combined-sources test.
4. Rollback: test-only; removing the target restores prior state.

## Open Questions

- Exact harness location (recast plugin test vs a navigation test target).
- Whether to also assert path reachability (start->end) or only walkable coverage.
