## 1. Offline builder and module

- [ ] 1.1 Add `NaviMeshBuilder : sky::AssetBuilder` and the `SkyNavigation.Builder` module (links only `Framework` + `Navigation`, no aurora, no direct recast)
- [ ] 1.2 Define the `.navmesh` source asset (JSON): scene reference + build params + export mode
- [ ] 1.3 Add offline scene/World loading: build a `World`, attach `NavigationSystem`, apply authored params
- [ ] 1.4 Drive generation through `NaviMeshFactory` / `NaviMeshGenerator`; write via `CollectTiles` + `SaveAsset`

## 2. Export modes

- [ ] 2.1 Select `Tiled` vs `Full` at build time from the source
- [ ] 2.2 Emit per-tile blobs + manifest for `Tiled`
- [ ] 2.3 Emit a single blob for `Full`
- [ ] 2.4 Keep `borderSize` stitching consistent across neighbouring tiles

## 3. Full runtime load

- [ ] 3.1 Serialize/restore a monolithic `dtNavMesh` for `Full`
- [ ] 3.2 `NaviMesh::LoadData` handles `Full`
- [ ] 3.3 Verify a full export loads as one nav mesh and queries correctly

## 4. Incremental rebuild

- [ ] 4.1 Support rebuilding a subset of tiles (world edit)
- [ ] 4.2 Verify rebuilt tiles stitch with untouched neighbours

## 5. Tests

- [ ] 5.1 Build a small scene and assert manifest/params round-trip
- [ ] 5.2 Load a subset of tiles and verify query works within the loaded region
