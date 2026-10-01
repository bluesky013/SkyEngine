## ADDED Requirements

### Requirement: Replication advances on a fixed network tick

Replication SHALL advance on a fixed network tick that is independent of the render frame rate, using an accumulator that is decoupled from frame delta.

#### Scenario: Frame rate does not change tick cadence

- **WHEN** the render frame rate varies
- **THEN** the number of network ticks per second SHALL remain at the configured fixed rate

### Requirement: Remote entities are rendered by interpolation

The client SHALL render remote replicated entities by interpolating between the two most recent received snapshots, rather than snapping to the latest value.

#### Scenario: Smooth remote motion

- **WHEN** the client has two consecutive snapshots for a remote entity
- **THEN** it SHALL interpolate between them for display

### Requirement: Network timing uses the real monotonic clock

The fixed network tick and its timing SHALL be driven by the real monotonic clock, not by the world simulation time, so that pausing or slowing the simulation does not corrupt network cadence.

#### Scenario: Simulation pause does not stall the network tick

- **WHEN** the world simulation is paused
- **THEN** the network tick SHALL continue to advance on real time

### Requirement: Catch-up is bounded

When the host falls behind, the accumulator SHALL bound catch-up work per frame to avoid an unbounded tick spiral.

#### Scenario: Backlog does not spiral

- **WHEN** the host has accumulated many pending network ticks
- **THEN** it SHALL process at most a configured maximum per frame and drop the excess backlog
