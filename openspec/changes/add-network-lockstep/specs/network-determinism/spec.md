## ADDED Requirements

### Requirement: Simulation uses a fixed deterministic step

The deterministic simulation SHALL advance on a fixed timestep with a bounded catch-up budget, independent of render frame rate and wall-clock time.

#### Scenario: Frame rate does not affect simulation

- **WHEN** the render frame rate varies
- **THEN** the simulation SHALL advance at the configured fixed rate with no wall-clock dependence

### Requirement: Deterministic iteration order

Simulation code SHALL iterate entities and components in a deterministic order stabilized by a stable key such as the entity id, and SHALL NOT depend on unordered container iteration or on the physical layout of dense arrays (for example `SparseSet` swap-remove reordering).

#### Scenario: Stable iteration across peers

- **WHEN** two peers simulate the same tick with the same inputs
- **THEN** they SHALL visit entities and components in the same order

#### Scenario: Layout change does not change order

- **WHEN** entities are removed and re-added so that underlying dense arrays reorder
- **THEN** the iteration order SHALL be unchanged for the same live entity set

### Requirement: Deterministic random number generation

Any randomness in the simulation SHALL come from a deterministic source seeded consistently across peers, and SHALL NOT use an unspecified global RNG.

#### Scenario: Same seed same result

- **WHEN** two peers simulate a tick with the same seed and inputs
- **THEN** they SHALL produce the same random draws

### Requirement: Deterministic math mode is required

The simulation SHALL run with a deterministic math mode and SHALL NOT enable fast-math or other floating-point behavior that varies across platforms.

#### Scenario: Non-deterministic mode rejected

- **WHEN** the simulation is configured with a non-deterministic math mode
- **THEN** the system SHALL reject the configuration

### Requirement: Divergence is detected

Peers SHALL exchange a simulation hash at a configured cadence, and a mismatch SHALL be reported.

#### Scenario: Mismatch detected

- **WHEN** two peers report different simulation hashes
- **THEN** the system SHALL report a desync and initiate recovery

### Requirement: Divergence triggers resynchronization

On a desync, the system SHALL resynchronize affected peers from authoritative state.

#### Scenario: Recovery from authoritative state

- **WHEN** a desync is confirmed
- **THEN** the affected peer SHALL be resynchronized from authoritative state before continuing
