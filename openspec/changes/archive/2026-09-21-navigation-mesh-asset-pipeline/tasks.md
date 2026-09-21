## 1. Asset data model

- [x] 1.1 Add `NaviMeshExportMode`, `NaviMeshBuildParams`, `NaviMeshTilePayload`, `NaviMeshData` in the interface module
- [x] 1.2 Binary serialization of the model (params + per-tile blobs + full blob)
- [x] 1.3 Record the export mode and build params (agent, resolution, bounds, maxSimplificationError, borderSize, version)

## 2. Interface tile emission

- [x] 2.1 Add `NaviMeshGenerator::GetBuildParams` and `CollectTiles(NaviMeshData&)` (backend implements)
- [x] 2.2 Recast backend emits per-tile blobs keyed by `(tx, ty, layer)` and its build params

## 3. Runtime load

- [x] 3.1 `NaviMesh::LoadData(NaviMeshData&)` (Tiled): rebuild params + tile cache, add tiles, build tiles, rebuild query
- [x] 3.2 Params consistency so the runtime reconstructs the same `dtNavMeshParams`/tile-cache params

## 4. Tests

- [x] 4.1 Tiled serialization round-trip
- [x] 4.2 Full serialization round-trip

## 5. Related backlog

- [ ] 5.1 `navigation-mesh-builder`: offline `.navmesh` source + scene cook, export modes (Tiled/Full), incremental rebuild, Full runtime load (split into its own change)
