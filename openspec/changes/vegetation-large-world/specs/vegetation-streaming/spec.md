## ADDED Requirements

### Requirement: Focus-driven cell paging with hysteresis

The vegetation sub-system SHALL page vegetation cells around a settable focus position, loading cells within a load radius and unloading cells beyond a larger unload radius, so the gap provides hysteresis, with cell bounds supplied by the surface provider (aligned to terrain tiles when the provider is terrain).

#### Scenario: Load cells near focus

- **WHEN** the streaming focus is set and streaming is enabled
- **THEN** cells within the load radius whose terrain surface is resident SHALL be requested for loading

#### Scenario: Unload cells beyond the radius

- **WHEN** a loaded cell's distance from the focus exceeds the unload radius
- **THEN** the cell SHALL be unloaded

#### Scenario: No thrash inside the hysteresis band

- **WHEN** the focus remains between a cell's load and unload radii
- **THEN** the cell SHALL remain loaded

### Requirement: Per-cell distance density LOD

Each cell SHALL select a density LOD band from its distance to the focus, reducing instance density with distance, with fades between bands so transitions are not visually abrupt.

#### Scenario: Density decreases with distance

- **WHEN** a cell is at a distance in a coarser density LOD band
- **THEN** its emitted density SHALL be lower than at a nearer band

#### Scenario: Fade between bands

- **WHEN** a cell's distance places it near a band boundary
- **THEN** the transition SHALL be blended rather than a hard change

### Requirement: Off-tick prefetch and per-tick budget

Cell input preparation SHALL run as an asynchronous task off the world tick, and completed cells SHALL be applied on the main thread within a configurable per-tick budget.

#### Scenario: Apply within budget

- **WHEN** more cells finish preparing than the per-tick budget allows
- **THEN** the sub-system SHALL apply at most the budget count this tick and the remainder on later ticks

### Requirement: Per-world isolation and invalidation

Each vegetation sub-system instance SHALL own its cells and placement state, and terrain tile changes SHALL invalidate the overlapping vegetation cells.

#### Scenario: Two worlds page independently

- **WHEN** two worlds each host a vegetation sub-system
- **THEN** loading or unloading cells in one world SHALL NOT change the cells of the other

#### Scenario: Invalidate on terrain change

- **WHEN** a terrain tile changes
- **THEN** the overlapping vegetation cells SHALL be invalidated and re-established from the updated surface
