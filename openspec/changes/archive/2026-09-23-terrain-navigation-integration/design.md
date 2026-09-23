## Context

Navigation builds nav meshes through a generator abstraction in `engine/navigation` (`NaviMeshGenerator`) that the recast backend implements (`RecastNaviMeshGenerator`). Geometry collection is the problem:

- `RecastNaviMeshGenerator::GatherGeometry` (`plugins/recast/src/RecastnaviMeshGenerator.cpp:70`) hardcodes a scan of world actors, keeps only those with a `phy::CollisionComponent`, and calls `shape->GetTriangleMesh()`; the resulting world-space meshes are inserted into a `NaviOctree`, then rasterized by `RecastTileGenerator` via `rcRasterizeTriangles`.
- Terrain now uses **heightfield** collision (`terrain-collision` / `btHeightfieldTerrainShape`), which yields no triangle mesh. Terrain collision objects are also created directly by the collision layer rather than living on actors with a `CollisionComponent`.
- So terrain is invisible to the nav mesh builder, and there is no seam for non-physics geometry sources.
- Reading the physics heightfield from navigation would be the wrong dependency direction: collision is a backend-specific representation, not a geometry source of record. `engine/terrain` already exposes authoritative LOD0 height samples, world-space tile addressing, and a stable render-free API (`TerrainField`, `TerrainSystem`).

Constraints: `engine/navigation` must stay free of terrain/render/physics types; recast and detour are vendored; navigation already has tile streaming, an octree, and incremental rebuild via `SetRebuildTiles`; terrain exposes per-tile LOD chains where LOD0 is the full-detail query/collision level.

## Goals / Non-Goals

**Goals:**

- Add a render- and physics-agnostic geometry-provider seam so nav mesh generation can consume sources beyond `CollisionComponent`.
- Feed terrain geometry to the nav mesh builder from the terrain core (LOD0), independent of the physics heightfield.
- Support offline cook and runtime/incremental rebuild, using LOD0, with correct tile granularity mapping.
- Keep `engine/navigation` terrain-free; the bridge owns the coupling.

**Non-Goals:**

- A custom recast heightfield span rasterizer as the primary path (a future optimization; triangles through the existing `rcRasterizeTriangles` path first).
- Navigation-aware terrain LOD (nav always uses LOD0; render LOD is irrelevant to nav).
- Dynamic obstacle carving, off-mesh links, or area annotations beyond slope-derived walkability.
- Replacing the existing collision-component geometry source (it stays, as an additional provider).

## Decisions

### D1: Navigation samples the terrain core, not physics

**Decision**: Nav geometry for terrain comes from `engine/terrain` LOD0 tile samples via a provider, never from the physics heightfield shape or the collision layer.

**Rationale**: Terrain is the source of record for the surface; physics collision is a backend-specific consumer. Sampling the core keeps navigation usable without physics, avoids coupling to Bullet/PhysX, and reuses the exact samples collision and gameplay query.

**Alternatives considered**: Expose the heightfield as triangles through `HeightFieldShape::GetTriangleMesh()` and keep the collision scan (couples nav to physics, duplicates geometry, and terrain colliders are not actor components). Read back from physics (latency, cost, coupling).

### D2: Geometry-provider seam in `engine/navigation`

**Decision**: Define `INaviGeometryProvider` plus a geometry sink in `engine/navigation`:

```
INaviGeometryProvider::Collect(const AABB &bounds, INaviGeometrySink &sink)
INaviGeometrySink::AddTriangles(const Vector3 *verts, uint32_t vertexCount,
                                const uint32_t *indices, uint32_t indexCount)
```

`NaviMeshGenerator` exposes a provider list; the recast backend's `GatherGeometry` consumes providers in addition to the existing collision-component scan. Providers are registered per world/generator.

**Rationale**: The current hardcoded scan is the only coupling point; a provider seam lets terrain (and future sources like splines/water) contribute geometry without the navigation core knowing about them, and without the backend hardcoding each source.

**Alternatives considered**: Special-case terrain inside recast (hardcodes terrain into the backend, breaks layering). Put terrain collection in the collision component path (requires terrain to be an actor component; it is not).

### D3: Terrain-to-triangles provider

**Decision**: A terrain provider (bridge linking `Terrain` + `Navigation`) implements `INaviGeometryProvider`. For a requested world-space `AABB`, it finds the overlapping terrain tiles (LOD0 resident), samples each tile's height grid, and emits two triangles per quad in world space:

```
worldXZ = TileToWorld(coord) + (ix, iz) * resolution
worldY  = meta.DecodeHeight(sample)
```

Quad winding is consistent with terrain up (+Y) so recast computes slopes correctly. The provider clips to the requested bounds at quad granularity.

**Rationale**: Leverages the terrain core's addressing and LOD0 samples; keeps geometry generation trivial and deterministic; fits the existing octree/rasterize path with no recast changes.

**Alternatives considered**: Prebuild a decimated "nav collision mesh" asset alongside terrain (extra pipeline, potential drift from terrain data). Direct span rasterization (see D8).

### D4: LOD0-only and residency

**Decision**: Nav builds always use terrain LOD0. Offline cook generates all LOD0 (or the build bounds' LOD0); runtime rebuild requires LOD0 tiles resident for the affected nav tiles and defers/retries a nav tile build when its terrain LOD0 is not yet streamed.

**Rationale**: Coarse LODs would produce inaccurate walkable surfaces and slopes, and would disagree with collision/query. LOD0 keeps nav, collision, and gameplay consistent.

**Alternatives considered**: Use the ring LOD (wrong surface). Use whatever LOD is resident (nondeterministic nav meshes).

### D5: Tile granularity mapping

**Decision**: Map nav build tiles (e.g. 10 m nav tiles) to terrain tiles (e.g. 64 m) by converting the nav tile's world `AABB` to the overlapping terrain tile range, then sampling only the needed sub-rect of each terrain tile's LOD0 grid. Nav tile size stays independent of terrain tile size.

**Rationale**: The two grids have different sizes and origins; sampling per nav tile keeps memory bounded and matches the existing nav tile streaming/rebuild model.

**Alternatives considered**: One nav tile per terrain tile (nav tile size becomes a terrain parameter; coarse for agents). Build the whole terrain at once (unbounded memory).

### D6: Incremental rebuild driven by terrain changes

**Decision**: Terrain tile changes (edit/generate/stream-in across a focus boundary) raise a change notification carrying the affected tile coordinate/bounds; the navigation system converts it to overlapping nav tiles and rebuilds them through the existing `SetRebuildTiles` path. Builds that only involve streamed-in LOD0 for already-known terrain do not trigger a rebuild unless the data differs.

**Rationale**: Reuses `navigation-tile-streaming`'s incremental rebuild; avoids full-world rebuilds on a single tile edit; keeps nav and terrain edits consistent.

**Alternatives considered**: Full rebuild on any change (expensive). Manual-only rebuild (stale nav after edits).

### D7: Provider registration owns the coupling

**Decision**: The terrain geometry provider is created and registered by a bridge (a module/target linking `Terrain` + `Navigation`), not by `engine/navigation` or the recast plugin. The navigation system holds an opaque provider list; recast consumes it through the seam.

**Rationale**: Preserves one-way layering: `engine/navigation` and `plugins/recast` stay terrain-free; only the bridge knows both.

**Alternatives considered**: Register from the terrain module (terrain would depend on navigation). Register from the recast plugin (backend depends on terrain).

### D8: Future optimization -- direct heightfield span rasterization

**Decision**: Record, but do not implement, a future path that writes terrain height samples directly into `rcHeightfield` spans (with slope-derived area flags) instead of emitting triangles, to cut the per-quad triangle overhead on large worlds.

**Rationale**: The triangle path is correct and simple and fits existing code; span rasterization is a performance optimization that can land later behind the same provider seam.

## Risks / Trade-offs

- [Triangle volume for large regions] -> Build per nav tile over clipped terrain sub-rects, not whole terrain; keep LOD0 only where needed; consider D8 if profiling shows a bottleneck.
- [LOD0 residency for offline vs runtime] -> Offline builder generates LOD0 for the bake bounds; runtime defers a nav tile build until its LOD0 is streamed, and retries.
- [Double geometry from collision + terrain] -> If terrain also had a triangle-mesh collision source, geometry could be counted twice; terrain uses heightfield collision (no triangle source), and the collision scan ignores heightfields, so no overlap.
- [Holes / non-walkable areas] -> Skip tiles with no data; optionally derive area flags from splat layers (water/cliff) for future masking.
- [Determinism] -> Terrain generation is deterministic (see `terrain-generation`), so provider output and nav meshes are reproducible and cacheable.
- [Build cost] -> Terrain nav builds are heavier than small collision meshes; run off-thread (existing task model) and budget per nav tile.
- [Provider lifetime/threading] -> Providers and the terrain read API must be safe to call from the build worker; read-only access to resident LOD0 tiles only.

## Migration Plan

1. Add `INaviGeometryProvider`/sink to `engine/navigation` and switch the recast `GatherGeometry` to consume providers plus the existing collision scan; no behavior change without providers.
2. Add a terrain read API (LOD0 iteration for world-space bounds) to the terrain core and a change notification on terrain tile edits.
3. Add the terrain geometry provider bridge and register it for worlds with terrain.
4. Add the terrain-change to nav-tile invalidation/rebuild path.
5. Rollback: providers are additive; removing the terrain provider restores the collision-only behavior.

## Open Questions

- Exact `INaviGeometryProvider` shape (bounds type, whether providers declare which nav tiles they affect, and whether the provider returns triangles or a generic span/heightfield structure).
- Whether the bridge is a dedicated target in `engine/terrain`, a new plugin, or part of the terrain module.
- Whether the terrain change notification is a subsystem event, a frame-diff, or pulled by the nav system each rebuild.
- Whether runtime rebuild is fully automatic or opt-in per world.
- Priority and timing of the D8 span-rasterization optimization.
