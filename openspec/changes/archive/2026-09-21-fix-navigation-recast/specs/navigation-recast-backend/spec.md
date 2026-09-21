## ADDED Requirements

### Requirement: Tile grid dimensions

The generator SHALL compute the tile-grid height from the grid height (`gh`) returned by `rcCalcGridSize`, not
from the grid width.

#### Scenario: Non-square bounds produce correct tile counts

- **WHEN** the build bounds produce different grid width and height
- **THEN** the number of tiles along the second axis SHALL be derived from `gh`

### Requirement: Navmesh and tile-cache tile capacity agree

The navmesh `maxTiles` SHALL be at least the tile-cache tile capacity, so every accepted tile can be added to the
navmesh.

#### Scenario: All built tiles fit the navmesh

- **WHEN** the tile cache accepts a tile
- **THEN** the navmesh SHALL have capacity for it

### Requirement: Tile data ownership

Tile data handed to the tile cache with `DT_COMPRESSEDTILE_FREE_DATA` SHALL have a single owner: on success the
tile cache owns it and the engine SHALL NOT free it; on failure the engine SHALL free it exactly once.

#### Scenario: Successful add does not double free

- **WHEN** `dtTileCache::addTile` succeeds with `DT_COMPRESSEDTILE_FREE_DATA`
- **THEN** the engine SHALL detach the buffer so its destructor SHALL NOT free it

#### Scenario: Failed add frees once

- **WHEN** `dtTileCache::addTile` fails
- **THEN** the engine SHALL free the buffer exactly once and SHALL NOT free it again on destruction

### Requirement: Tile cache lifetime

The generator SHALL free its `dtTileCache` on destruction and on initialization failure.

#### Scenario: Generator destruction frees the tile cache

- **WHEN** the generator is destroyed
- **THEN** `dtFreeTileCache` SHALL be called on the owned tile cache

### Requirement: Allocation and null safety

The backend SHALL check allocations before use and clean up on failure: `dtAllocNavMesh` and
`dtAllocNavMeshQuery`.

#### Scenario: Navmesh allocation failure

- **WHEN** `dtAllocNavMesh` returns null or nav mesh init fails
- **THEN** the build SHALL fail gracefully without dereferencing null and without leaking the nav mesh

#### Scenario: Query allocation failure

- **WHEN** `dtAllocNavMeshQuery` returns null or query init fails
- **THEN** no null dereference SHALL occur and the query SHALL be freed

### Requirement: Render-agnostic core

The navigation core SHALL NOT depend on any renderer. `NaviMesh` / `RecastNaviMesh` SHALL NOT include render
headers or link a render module, and debug output SHALL be exposed as render-agnostic geometry through
`NaviMesh::BuildDebugGeometry(NaviDebugGeometry &)`, which any renderer MAY consume later.

#### Scenario: No render dependency in the build/query core

- **WHEN** the recast backend is built
- **THEN** it SHALL link only `Navigation`, `Physics`, and the recast third-party, with no render module

#### Scenario: Debug geometry is render-agnostic data

- **WHEN** `BuildDebugGeometry` is called on a built nav mesh
- **THEN** it SHALL return a triangle list of positions and colors without referencing any renderer

#### Scenario: Build does not require a renderer

- **WHEN** the nav mesh build runs with no render subsystem attached
- **THEN** it SHALL complete without any render calls

### Requirement: Path query correctness

`FindPath` SHALL test the end polygon result, SHALL require a valid query filter and nav query, and SHALL treat
zero or failed results as failure.

#### Scenario: End polygon not found

- **WHEN** the end position has no nearby polygon
- **THEN** `FindPath` SHALL return `FAILED`

#### Scenario: Null filter or query

- **WHEN** the filter is null or the nav query is not initialized
- **THEN** `FindPath` SHALL return `FAILED` without dereferencing null
