## 1. Tile-aware builder

- [ ] 1.1 Add `NaviMeshBuilder : sky::AssetBuilder` and the `SkyNavigation.Builder` module (links only `Framework` + `Navigation`, no aurora, no direct recast)
- [x] 1.2 Extend the navigation interface with a per-tile payload emission entry (backend implements it)
- [ ] 1.3 Drive generation through `NaviMeshFactory` / `NaviMeshGenerator` (backend-agnostic)
- [x] 1.4 Generate per-tile nav mesh blobs and tile-cache layer blobs keyed by `(tx, ty, layer)`
- [ ] 1.5 Add a configurable export mode (`Tiled` vs `Full`) selected at build time
- [ ] 1.6 Keep `borderSize` stitching consistent across neighbouring tiles

## 2. Manifest and build params

- [ ] 2.1 Emit a tile manifest (coords, bounds, layer count, data offset/size, connectivity)
- [ ] 2.2 Persist fixed build params (agent cfg, resolution, tile size, grid origin, world bounds, version)
- [ ] 2.3 Validate that the runtime reconstructs identical `dtNavMeshParams`

## 3. Runtime load entry

- [ ] 3.1 Add an API to add tiles from the asset into a nav mesh / tile cache (tiled)
- [ ] 3.2 Add a full-export load path (single blob, no paging)
- [ ] 3.3 Verify a subset of tiles can be loaded without reading the whole asset (tiled)
- [ ] 3.4 Verify a full export loads as one nav mesh and queries correctly

## 4. Incremental rebuild

- [ ] 4.1 Support rebuilding a subset of tiles (world edit)
- [ ] 4.2 Verify rebuilt tiles stitch with untouched neighbours

## 5. Tests

- [ ] 5.1 Build a small scene and assert manifest/params round-trip
- [ ] 5.2 Load a subset of tiles and verify query works within the loaded region

## 6. Related backlog

- [ ] 6.1 `navigation-tile-streaming`: paging consumes this tile layout (split into its own change)
- [ ] 6.2 `navigation-async-load`: off-thread tile reading (split into its own change)
