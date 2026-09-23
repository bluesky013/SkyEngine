# terrain-streaming Specification

## Purpose
TBD - created by archiving change terrain-large-world-core. Update Purpose after archive.
## Requirements
### Requirement: Focus-driven proximity paging with hysteresis

The terrain sub-system SHALL page tiles around a settable focus position, loading tiles within a load radius and unloading tiles beyond a larger unload radius, so that the gap between the radii provides hysteresis.

#### Scenario: Load tiles near focus

- **WHEN** the streaming focus is set to a position and streaming is enabled
- **THEN** tiles within the load radius whose payloads are available in the manifest SHALL be requested for loading

#### Scenario: Unload tiles beyond the unload radius

- **WHEN** a loaded tile's distance from the focus exceeds the unload radius
- **THEN** the tile SHALL be removed from the runtime field

#### Scenario: No thrash inside the hysteresis band

- **WHEN** the focus remains between the load radius and the unload radius of an already loaded tile
- **THEN** the tile SHALL remain loaded

### Requirement: LOD-aware paging

The sub-system SHALL page tiles per terrain LOD level, where LOD `q` is loaded within an annulus bounded by a per-LOD radius that grows with `q`, and SHALL track loaded and pending state by tile coordinate and LOD so that a coordinate may hold several LODs during transitions.

#### Scenario: Load coarser LODs farther out

- **WHEN** streaming pages a tile whose distance falls in a coarser LOD's annulus
- **THEN** it SHALL request the tile at that LOD rather than always at the highest-detail LOD

#### Scenario: Near tiles use highest detail

- **WHEN** streaming pages a tile within the innermost LOD radius
- **THEN** it SHALL request LOD0

#### Scenario: Per-LOD unload

- **WHEN** a loaded (coordinate, LOD) entry leaves that LOD's unload radius
- **THEN** that entry SHALL be unloaded independently of other LODs of the same coordinate

### Requirement: Off-tick asynchronous tile prefetch

Tile payload preparation SHALL run as an asynchronous task off the world tick, and the completed payload SHALL be applied to the runtime field on the main thread within a configurable per-tick apply budget.

#### Scenario: Apply finished loads within budget

- **WHEN** more tile payloads finish prefetching than the per-tick budget allows
- **THEN** the sub-system SHALL apply at most the budget count this tick and apply the remainder on subsequent ticks

### Requirement: Pending load cancellation and payload failure tolerance

The sub-system SHALL cancel pending tile loads whose tiles move outside the unload radius before application, and SHALL tolerate missing or stale payloads without failing.

#### Scenario: Cancel a drifted pending load

- **WHEN** a pending tile's distance from the focus exceeds the unload radius before its payload is applied
- **THEN** the pending load SHALL be cancelled and the tile SHALL not be added to the field

#### Scenario: Missing payload

- **WHEN** a requested tile payload cannot be produced or is unavailable
- **THEN** the sub-system SHALL continue paging remaining tiles without error

### Requirement: Per-world tile state isolation

Each terrain sub-system instance SHALL own its tile manifest, loaded set (keyed by coordinate and LOD), and pending set, so that multiple worlds do not share terrain tile state.

#### Scenario: Two worlds page independently

- **WHEN** two worlds each host a terrain sub-system
- **THEN** loading or unloading tiles in one world SHALL NOT change the loaded tiles of the other

### Requirement: Invalidation on terrain change

When the terrain data or manifest changes, the sub-system SHALL invalidate the loaded set so that paging re-establishes the correct tiles on subsequent ticks.

#### Scenario: Invalidate after manifest change

- **WHEN** the terrain asset or manifest is replaced
- **THEN** the loaded set SHALL be cleared and tiles SHALL be re-paged against the new manifest

