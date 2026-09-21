## Context

Nav meshes are built at runtime and never persisted, so there is nothing to load or stream. This change adds the
backend-agnostic **asset data contract** and a runtime load path so a nav mesh can be persisted and restored.
The offline production of that asset (scene cook) is a separate concern split into `navigation-mesh-builder`, since
it needs offline scene/World loading infrastructure that does not exist in the framework today.

## Goals / Non-Goals

**Goals**
- A backend-agnostic nav mesh asset payload (`NaviMeshData`) covering Tiled and Full export modes.
- Build params persisted so the runtime reconstructs identical Detour parameters.
- An interface-level tile emission entry so tooling never touches backend types.
- A runtime load entry that restores a Tiled nav mesh (nav mesh + tile cache + query).

**Non-Goals**
- The offline builder / `.navmesh` source / scene cook, export-mode selection at build time, incremental tile
  rebuild, and Full (monolithic) runtime load. These are `navigation-mesh-builder`.
- Tile paging, async loading, async queries (separate backlog changes).

## Decisions

### D1. Asset payload lives in the interface module
`NaviMeshData` / `NaviMeshBuildParams` / `NaviMeshTilePayload` / `NaviMeshExportMode` live in `engine/navigation`
and contain no backend types. `NaviMeshData` has explicit `Save`/`Load(BinaryArchive&)`.

### D2. Interface-level tile emission
`NaviMeshGenerator` exposes `GetBuildParams()` and `CollectTiles(NaviMeshData&)`. The recast backend implements
them from its `rcConfig` and per-tile generators, so the builder never includes recast types.

### D3. Params consistency
`NaviMeshBuildParams` carries agent config, resolution, bounds, `maxSimplificationError`, `borderSize`, and a
version. The runtime `LoadData` derives `dtNavMeshParams` and `dtTileCacheParams` from these, so a persisted mesh
rebuilds identically.

### D4. Runtime load is Tiled-first
`NaviMesh::LoadData` supports `Tiled`: it rebuilds the nav mesh, creates the tile cache, adds each tile blob
(copying into `dtAlloc` memory with `DT_COMPRESSEDTILE_FREE_DATA`), builds the nav mesh tiles, and rebuilds the
query. `Full` load is deferred to the builder change (needs `dtNavMesh` serialization).

### D5. Render-agnostic (continues `fix-navigation-recast`)
The asset model and load path carry no render dependency; debug geometry stays render-agnostic.

## Risks / Trade-offs

- [`Full` mode exists in the model but has no runtime load yet] -> modeled as a field only; `LoadData` returns
  false for `Full` until `navigation-mesh-builder` adds `dtNavMesh` serialization.
- [Tile blob ownership on load] -> copies into `dtAlloc` memory and passes `DT_COMPRESSEDTILE_FREE_DATA`, matching
  the build path's single-owner rule.

## Open Questions

- Whether `Full` should reuse the tiled payloads to rebuild a monolithic mesh, or carry a serialized `dtNavMesh`.
