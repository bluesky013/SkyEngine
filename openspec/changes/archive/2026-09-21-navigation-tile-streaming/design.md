## Context

`navigation-mesh-asset-pipeline` defined the tiled asset manifest and Tiled runtime load; `navigation-mesh-builder`
produces per-tile assets. Loading still adds every tile up front and never unloads. This change adds runtime paging
driven by a focus position with load/unload radii, and makes tile-cache state per-instance so worlds do not share
global state.

## Goals / Non-Goals

**Goals**
- Add/remove a single tile at runtime from the nav mesh and its tile cache.
- Page tiles by proximity (load radius + unload radius hysteresis) from a focus position.
- Track available vs loaded tiles per world.
- Remove process-global tile-cache allocator/mesh-processor state.
- Invalidate on nav mesh change.

**Non-Goals**
- Async/off-thread tile loading (`navigation-async-load`) and the camera binding of the focus (aurora integration).
- LOD/merged far tiles.

## Decisions

### D1. Per-tile add/remove API
`NaviMesh` gains `PrepareStreaming(buildParams)` (empty nav mesh + tile cache), `AddTile(payload)`, and
`RemoveTile(coord)`. `LoadData` (Tiled) is implemented as `PrepareStreaming` + `AddTile` per payload.

### D2. Manifest-driven paging in NavigationSystem
`NavigationSystem::SetupStreaming(NaviMeshData)` stores the manifest (`(tx,ty)` -> payload) and initializes an empty
mesh; `Tick` calls `UpdateStreaming`, which loads tiles inside the load radius and unloads tiles beyond the unload
radius (hysteresis) around `focus`.

### D3. Focus is an explicit input
The focus position is set via `SetStreamingFocus`; binding it to the active camera is deferred to
`aurora-navigation-integration`, keeping this change render-independent.

### D4. Per-instance tile-cache state
`RecastNaviMesh` owns its `dtTileCacheAlloc` and `RecastTileCacheMeshProcessor` as members instead of function-local
statics, so two worlds (or editor + game) do not share state.

### D5. Invalidation
`NavigationSystem::OnNavMeshChanged` clears the loaded set; the next tick re-pages against the updated manifest.

## Risks / Trade-offs

- [Paging scan is O(available) per unload pass] -> acceptable for now; a spatial index is a follow-up.
- [Rapid movement can outrun synchronous loads] -> tiles load synchronously per tick; async loading is
  `navigation-async-load`.
- [Focus is externally driven] -> without a driver the system pages around the origin; the integration change wires
  the camera.

## Open Questions

- Whether to retain near tiles on teleport (avoid a full unload/load spike).
- Paging budget per tick (tiles loaded per frame).
