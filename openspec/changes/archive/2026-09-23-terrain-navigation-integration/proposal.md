## Why

The navigation subsystem builds nav meshes from geometry gathered by the recast backend, but `RecastNaviMeshGenerator::GatherGeometry` is hardcoded to scan `phy::CollisionComponent` actors and call `PhysicsShape::GetTriangleMesh()`. Terrain is now moving to per-tile **heightfield** collision (`terrain-collision`, backed by `btHeightfieldTerrainShape`), which produces no triangle mesh, and terrain collision objects are created directly rather than living on actors with a `CollisionComponent`. As a result terrain is invisible to the nav mesh builder, and there is no seam for any non-physics geometry source.

The correct dependency direction is also wrong if navigation reads the physics heightfield: physics collision is backend-specific and not a geometry source of record. The terrain core (`engine/terrain`) already exposes authoritative LOD0 height samples and world-space addressing, so navigation should consume terrain from the core, not from physics.

## What Changes

- Add a backend-agnostic **geometry provider** seam to `engine/navigation` so nav mesh generation can consume geometry from sources other than `CollisionComponent` triangle meshes.
- Make the recast backend gather geometry from registered providers in addition to the existing collision-component scan.
- Add a **terrain geometry provider** (bridge linking `Terrain` + `Navigation`) that emits world-space triangles from terrain LOD0 tile samples for a requested build region, without depending on render, RHI, or the physics heightfield.
- Require navigation builds to use terrain **LOD0** (never the render clipmap LOD) and ensure LOD0 residency for the build region in both offline cook and runtime rebuild.
- Drive **incremental nav mesh rebuild** from terrain changes: when a terrain tile changes, invalidate the overlapping nav tiles and rebuild them via the existing `SetRebuildTiles` path.
- Keep `engine/navigation` free of terrain and render types; the bridge owns the coupling.

## Capabilities

### New Capabilities

- `navigation-geometry-provider`: a render- and physics-agnostic geometry-provider seam in `engine/navigation`, consumed by the recast backend alongside the existing collision-component source.
- `terrain-navigation-geometry`: terrain-as-geometry provider that samples terrain LOD0 core data over a build region and emits world-space triangles (or spans) to the nav mesh builder.
- `navigation-terrain-rebuild`: LOD0 residency for nav builds plus incremental nav-tile invalidation and rebuild when terrain tiles change.

### Modified Capabilities

<!-- No navigation capabilities exist under openspec/specs/; navigation specs live only in openspec/changes/archive. Terrain has no main specs either. -->

## Impact

- **`engine/navigation`**: add `INaviGeometryProvider` (and a geometry sink abstraction); `NaviMeshGenerator` surfaces registered providers to backends; no terrain/render/physics types introduced.
- **`plugins/recast`**: `RecastNaviMeshGenerator::GatherGeometry` also consumes providers, feeding the existing `NaviOctree` / `rcRasterizeTriangles` path.
- **New bridge**: a terrain geometry provider linking `Terrain` + `Navigation`; registered with the world's navigation system (or the generator) so the backend sees it without depending on terrain.
- **`plugins/terrain` / `engine/terrain`**: terrain core must expose LOD0 tile sample iteration for a world-space bounds (a small read API over `TerrainSystem`/`TerrainField`); terrain tile edits raise a change notification consumed by the rebuild path.
- **`terrain-large-world-core`**: depends on / is depended on by this change only via the proposed read API and change notification; the terrain change's open question about navigation geometry is resolved to "sample the terrain core directly".
- No new third-party dependencies; recast and detour are already vendored.
