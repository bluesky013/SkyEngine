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

### Requirement: Owned layered manifest

Streaming SHALL own payloads grouped by coordinate and preserve every layer. The per-frame budget SHALL count coordinates. Replacing a manifest or detaching a world SHALL NOT leave work referring to released payloads.

#### Scenario: Caller releases source

- **WHEN** the caller releases setup data
- **THEN** later paging SHALL remain valid

#### Scenario: Layered coordinate

- **WHEN** one coordinate contains multiple layers
- **THEN** paging SHALL load all those layers

#### Scenario: Replace manifest

- **WHEN** setup replaces a manifest
- **THEN** stale data SHALL NOT be installed into the new mesh

### Requirement: Query invalidation before streaming mutation

Streaming SHALL cancel in-flight path results before adding or removing tile geometry, while worker tasks retain their inputs until actual completion.

#### Scenario: Tile mutation during a path query

- **WHEN** paging changes mesh tiles while a path query is in flight
- **THEN** the query SHALL be marked canceled and its completed output SHALL be invalid

### Requirement: Failed coordinates retain bounded retry state

Untried desired coordinates SHALL take precedence over retries. Failures SHALL back off with a bounded retry delay so a permanently invalid nearest coordinate cannot consume every frame's installation budget. An unsuccessful partial-coordinate rollback or unload SHALL remain tracked and be retried before adding that coordinate's layers again, even after focus moves away. Coordinate removal SHALL be idempotent on an initialized mesh so an already-absent first-layer failure cannot permanently block retry. Pending load count SHALL describe desired coordinates awaiting installation, including retry delay, rather than disk IO.

#### Scenario: Bad nearest coordinate

- **WHEN** the coordinate budget is one and the nearest coordinate repeatedly fails
- **THEN** farther healthy desired coordinates SHALL still be installed and repeated failures SHALL not be attempted every tick

#### Scenario: Rollback itself fails

- **WHEN** a layer installation fails and removal also fails
- **THEN** the runtime SHALL retain cleanup state and confirm removal before attempting a new installation for the coordinate

#### Scenario: First layer fails without installing geometry

- **WHEN** first-layer installation fails and leaves the coordinate absent
- **THEN** cleanup SHALL confirm absence successfully and a later retry SHALL be allowed after the failure cause is removed

