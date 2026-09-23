## 1. Navigation geometry-provider seam

- [x] 1.1 Define `INaviGeometryProvider` (collect world-space geometry for a world `AABB`) and a geometry sink (`AddTriangles`) in `engine/navigation`
- [x] 1.2 Add an opaque provider list to `NaviMeshGenerator` / `NavigationSystem` with register/unregister
- [x] 1.3 Confirm the navigation core headers introduce no terrain, render, or physics includes

## 2. Terrain read API and change notification

- [x] 2.1 Add a terrain core read API to iterate resident LOD0 tile samples over a world-space `AABB` (with a coverage/incomplete signal)
- [x] 2.2 Add a terrain tile-change notification (tile coordinate/bounds) emitted on edit/generate/data change
- [x] 2.3 Add tests for bounds-to-tile-range mapping and for the coverage signal with missing LOD0

## 3. Terrain geometry provider (bridge)

- [x] 3.1 Create the bridge (target linking `Terrain` + `Navigation`, no render/physics) and implement `INaviGeometryProvider`
- [x] 3.2 Emit two world-space triangles per terrain quad using metadata addressing and decoded LOD0 heights
- [x] 3.3 Use consistent upward winding and clip emitted quads to the requested bounds
- [x] 3.4 Map nav build tiles to terrain tiles by world-space overlap and sample only the needed sub-rect
- [x] 3.5 Report incomplete coverage when overlapping LOD0 tiles are not resident
- [x] 3.6 Register the provider with the world's navigation system for worlds that host terrain

## 4. Recast backend consumption

- [x] 4.1 Make `RecastNaviMeshGenerator::GatherGeometry` also gather from registered providers into the `NaviOctree`
- [x] 4.2 Preserve the existing collision-component triangle-mesh source unchanged
- [x] 4.3 Confirm a build with no providers behaves exactly as before

## 5. LOD0 residency and incremental rebuild

- [x] 5.1 Require terrain LOD0 for the affected region and defer/retry a nav tile build when it is missing
- [x] 5.2 Convert terrain tile changes to overlapping nav tiles and rebuild them via `SetRebuildTiles`
- [x] 5.3 Avoid rebuilds when streamed-in LOD0 matches the data nav was built from
- [x] 5.4 Ensure offline cook and runtime rebuild both use LOD0 and never the render clipmap LOD

## 6. Tests and validation

- [x] 6.1 Add a test for incremental rebuild limited to overlapping nav tiles
- [x] 6.2 Add a determinism test (same terrain data + config -> identical nav tile payload)
- [x] 6.3 Build the engine and run navigation/recast tests with no regressions
