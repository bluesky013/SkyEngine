## Why

The tile manifest and Tiled runtime load exist (`navigation-mesh-asset-pipeline`) and the builder produces
per-tile assets (`navigation-mesh-builder`), but the runtime still loads every tile at once and never unloads any.
This change adds runtime tile paging: tiles load and unload by proximity so memory tracks the active area instead
of the whole world.


The recast backend builds per-tile, but there is no runtime tile lifecycle: `PrepareTileCache` adds every tile up
front and `NavMesh` is never asked to unload one. Memory therefore grows with world size, large/open worlds are
not viable, and the shared `static` allocator / mesh processor / compressor block per-world isolation. This change
makes tiles first-class runtime objects that load and unload by proximity.

## What Changes

- Manage tiles individually at runtime: add/remove a tile from the nav mesh and the tile cache
  (`dtNavMesh::removeTile`, tile-cache add/remove) instead of loading everything at build time.
- Page tiles by proximity to the active listener(s) with a load radius and an unload radius (hysteresis), driven
  by the world tick.
- Source tiles from a stored nav mesh asset rather than an in-memory build (depends on the mesh asset pipeline).
- Remove process-global navigation state so multiple worlds (or editor + game) do not share an allocator/processor.
- Invalidate and rebuild affected tiles when the world changes, and drive `NavigationSystem::OnNavMeshChanged`.

## Capabilities

### New Capabilities
- `navigation-tile-streaming`: per-tile add/remove, proximity paging, per-world isolation, dynamic tile rebuild.

## Prerequisite / related

- **Depends on `navigation-mesh-asset-pipeline`**: per-tile addressing, the manifest, and the fixed build params
  must be produced by the **generation/build stage** before paging can load a subset. Runtime paging cannot be
  added without a tile-aware builder. Assets exported in `Full` mode are loaded whole and bypass paging.
- `navigation-async-load`: the off-thread load mechanism that feeds tile paging.

## Impact

- `plugins/recast` (generator, nav mesh, tile cache ownership), `engine/navigation` (`NaviMesh`, `NavigationSystem`
  tick, per-world state).

## Open Questions (expand later)

- Paging driver and policy (camera vs audio listener vs an explicit streamer); tile size vs page granularity.
- Whether tile LOD / merged low-res tiles are needed for far distances.
