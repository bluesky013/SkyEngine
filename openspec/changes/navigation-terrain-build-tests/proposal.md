## Why

`terrain-navigation-integration` delivered the render-agnostic core for building nav meshes from terrain: a geometry provider that emits terrain LOD0 triangles, LOD0 residency/deferral, and incremental rebuild coordination. Those are covered by unit-level tests (provider triangles, coordinator mapping/defer/determinism). What was deferred is the **end-to-end validation through the recast backend**: actually building a nav mesh from terrain LOD0 and asserting walkability, and building from terrain plus collision geometry together. That needs a World-level harness (World + terrain system + recast factory), which is out of scope for the core change.

## What Changes

- Add a World-level integration test harness that can build a nav mesh from geometry sources (terrain provider and/or collision components) using the recast backend.
- Add an end-to-end test: terrain LOD0 produces a nav mesh with the expected walkable area over flat and sloped terrain.
- Add an end-to-end test: terrain provider geometry and collision-component geometry combine into one nav mesh build.

## Capabilities

### New Capabilities

- `navigation-terrain-build`: end-to-end nav mesh construction from terrain LOD0 (flat/sloped walkability) and from combined geometry sources (terrain + collision).

### Modified Capabilities

<!-- none -->

## Impact

- Adds tests/harness only (no engine behavior changes); likely a test target under the navigation/recast side.
- Depends on `terrain-navigation-integration` (provider seam, coordinator) and the recast backend.
- No new third-party dependencies.
