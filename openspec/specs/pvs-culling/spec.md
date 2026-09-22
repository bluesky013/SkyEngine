# pvs-culling Specification

## Purpose
TBD - created by archiving change pvs-implementation-review. Update Purpose after archive.
## Requirements
### Requirement: PVS grid configuration and coordinate math

The PVS subsystem SHALL map world positions to cells and sectors, and cells to sector-local indices, using a
`PVSConfig` (world offset, per-axis cell size, cells-per-sector XZ, cells-per-chunk) with floor-based coordinate
conversion that is correct for negative coordinates.

#### Scenario: Negative coordinate conversion
- **WHEN** a world position with negative components is converted to a cell/sector coordinate
- **THEN** the result SHALL use floor division (e.g. `-1` for a value in `[-1, 0)`), not truncation toward zero

#### Scenario: Round-trip through cell world minimum
- **WHEN** a position is converted to a cell coordinate and that coordinate's world minimum is converted back
- **THEN** the same cell coordinate SHALL result, and the original position SHALL lie inside the cell extent

### Requirement: Sector streaming with load/unload hysteresis

The PVS loader SHALL stream sectors in around the viewer and unload them using a separate, larger radius so that a
viewer moving across a sector boundary does not repeatedly load and unload the same sectors.

#### Scenario: Hysteresis band prevents thrashing
- **WHEN** the viewer oscillates around a sector boundary within the hysteresis band
- **THEN** sectors inside the load radius SHALL remain loaded and SHALL NOT be unloaded

#### Scenario: Distant sectors are unloaded
- **WHEN** a loaded sector's Chebyshev distance from the current sector exceeds the load radius plus the unload
  margin
- **THEN** that sector SHALL be unloaded

#### Scenario: Missing sector is recorded
- **WHEN** a sector within the load radius has no backing file on disk
- **THEN** the loader SHALL record the miss and continue without failing the update

### Requirement: Visibility query contract

The runtime SHALL answer per-object visibility queries from the loaded PVS data for the sector that contains the
queried cell (including streamed neighbor sectors), and SHALL fail safe: an object whose visibility cannot be
determined (no containing sector loaded, out-of-range cell, or null data) SHALL be treated as visible rather than
culled.

#### Scenario: Visible object
- **WHEN** a query is made for an object whose bit is set in the cell visibility bitset
- **THEN** the query SHALL report the object visible

#### Scenario: Culled object
- **WHEN** a query is made for an object whose bit is clear in a valid, loaded cell bitset
- **THEN** the query SHALL report the object not visible

#### Scenario: Neighbor sector cell
- **WHEN** a query is made for a cell inside a loaded neighbor sector (outside the main-view sector)
- **THEN** the query SHALL be answered from that neighbor sector rather than dropped

#### Scenario: Unavailable data does not cull
- **WHEN** the visibility data for the query is unavailable (no loaded sector, out-of-range cell, or null data)
- **THEN** the query SHALL report the object visible and SHALL NOT dereference null or out-of-range memory

#### Scenario: Unknown or invalid object id
- **WHEN** a query is made for the reserved invalid object id
- **THEN** the system SHALL treat it as not valid and SHALL NOT index the bitset with it

### Requirement: Ownership and lifetime safety

PVS runtime objects SHALL have unambiguous ownership: culling view data SHALL be returned as a reference-counted
object owned by the caller (the render scene adopts it via `CounterPtr`).

#### Scenario: Inactive culling is safe to query
- **WHEN** the culling system has not been initialized (no loader) and a view is prepared
- **THEN** it SHALL return a safe result and SHALL NOT dereference the missing loader

### Requirement: Plugin build hygiene

The PVS plugin SHALL build only the targets it actually ships, SHALL NOT include headers via paths that escape the
plugin directory, and SHALL keep its sources scope-balanced.

#### Scenario: Only shipped targets are built
- **WHEN** the project is configured with or without editor mode
- **THEN** the plugin SHALL build its core / render / module targets and SHALL NOT register the removed legacy
  editor target

#### Scenario: No plugin-escaping includes
- **WHEN** the PVS plugin sources are compiled
- **THEN** they SHALL NOT include headers via relative paths that escape the plugin directory

### Requirement: Render-independent core

PVS grid/config, sector storage, streaming and the visibility query SHALL live in a build target that does not
depend on the legacy render stack or the editor framework, so they can be built and tested independently.

#### Scenario: Core built without the legacy render stack
- **WHEN** the render-independent PVS core target is built
- **THEN** it SHALL link only the core framework libraries and SHALL NOT link `RenderCore` or any editor target

#### Scenario: Legacy glue isolated
- **WHEN** the legacy `IRenderSceneCulling` integration is compiled
- **THEN** it SHALL live in a separate target that depends on the core, so removing it does not affect the core

### Requirement: PVS data serialization owned by the core

The core SHALL own both directions of the PVS on-disk format: header/config and sector read (deserialize) and
write (serialize), through provider/sink abstractions that do not require the render or editor layers. A
save-then-load round trip SHALL reproduce the configuration and sector data.

#### Scenario: Config round trip
- **WHEN** a `PVSConfig` is serialized and then deserialized
- **THEN** all fields SHALL be preserved

#### Scenario: Sector round trip
- **WHEN** a `PVSSector` (version, chunk size, cells and chunk storage) is serialized and then deserialized
- **THEN** the version, chunk size, cell table and chunk bytes SHALL be preserved

#### Scenario: Write sink abstraction
- **WHEN** PVS data is written
- **THEN** it SHALL go through a writer abstraction (`IPVSSectorWriter`) that the core provides, without depending
  on the removed editor bake pipeline

### Requirement: Testable core without the legacy bake path

PVS grid, streaming, query and serialization logic SHALL be testable without loading or linking the legacy render
bake pipeline.

#### Scenario: Math, streaming, query and serialization tests
- **WHEN** the PVS tests run
- **THEN** they SHALL cover coordinate math, streaming hysteresis, neighbor-sector/missing-sector queries, query
  fallback and serialization round-trips without requiring the legacy bake path to be exercised

