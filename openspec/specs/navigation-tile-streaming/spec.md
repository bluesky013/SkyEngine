# navigation-tile-streaming Specification

## Purpose
TBD - created by archiving change navigation-tile-streaming. Update Purpose after archive.
## Requirements
### Requirement: Per-tile runtime add and remove

The nav mesh SHALL support adding and removing a single tile at runtime, affecting both the nav mesh and its tile
cache, and SHALL support initializing an empty streamable mesh from build params.

#### Scenario: Add a tile at runtime

- **WHEN** a persisted tile payload is added to a streamable mesh
- **THEN** the tile SHALL be present in the tile cache and the nav mesh

#### Scenario: Remove a tile at runtime

- **WHEN** a tile coordinate is removed
- **THEN** the tile SHALL be removed from both the nav mesh and the tile cache

#### Scenario: Streamable mesh starts empty

- **WHEN** a mesh is prepared for streaming with build params
- **THEN** it SHALL have a nav mesh, tile cache, and query but no tiles

### Requirement: Manifest-driven proximity paging

`NavigationSystem` SHALL page tiles around a focus position using a load radius and a larger unload radius
(hysteresis), driven from the world tick.

#### Scenario: Tiles near the focus load

- **WHEN** the focus is set near available tiles and streaming is enabled
- **THEN** tiles within the load radius SHALL be loaded

#### Scenario: Distant tiles unload

- **WHEN** the focus moves so a loaded tile is beyond the unload radius
- **THEN** that tile SHALL be unloaded

#### Scenario: Hysteresis prevents thrashing

- **WHEN** a tile is between the load and unload radii
- **THEN** its loaded/unloaded state SHALL be retained

### Requirement: Loaded and available tile tracking

The system SHALL track which tiles are available in the manifest and which are currently loaded.

#### Scenario: Loaded count reflects paging

- **WHEN** tiles are added or removed by paging
- **THEN** the loaded-tile count SHALL reflect the current set

### Requirement: Per-world isolation

Tile-cache allocator and mesh-processor state SHALL be per nav mesh instance, not process-global.

#### Scenario: Two meshes do not share state

- **WHEN** two nav meshes exist (for example two worlds)
- **THEN** each SHALL own its tile-cache allocator and mesh processor

### Requirement: Invalidation on mesh change

`NavigationSystem::OnNavMeshChanged` SHALL invalidate the loaded set so paging re-evaluates against the manifest.

#### Scenario: Mesh change resets paging

- **WHEN** the nav mesh changes
- **THEN** the loaded set SHALL be cleared

