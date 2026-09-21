## 1. Per-tile lifecycle

- [x] 1.1 Expose runtime add/remove of a nav mesh tile and its tile-cache tile
- [x] 1.2 Stop adding all tiles up front; make initial load a paged subset
- [x] 1.3 Track loaded vs available tiles per world

## 2. Proximity paging

- [x] 2.1 Choose a paging driver (camera / listener) and load + unload radii with hysteresis
- [x] 2.2 Integrate paging into `NavigationSystem::Tick`
- [ ] 2.3 Handle rapid movement (async load lag, retention of near tiles)

## 3. Per-world isolation

- [x] 3.1 Remove process-global allocator / mesh processor / compressor state
- [ ] 3.2 Verify two worlds (or editor + game) do not share navigation state

## 4. Dynamic updates

- [x] 4.1 Rebuild/invalidate affected tiles on world change
- [x] 4.2 Drive `NavigationSystem::OnNavMeshChanged`

## 5. Related backlog

- [ ] 5.1 `navigation-mesh-asset-pipeline`: persisted tiles to stream (split into its own change)
- [ ] 5.2 `navigation-async-load`: off-thread tile loading mechanism (split into its own change)
