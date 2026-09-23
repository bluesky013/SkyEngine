## ADDED Requirements

### Requirement: Capture world state

A physics world SHALL capture its simulation state into a backend-neutral snapshot addressed by stable `PhysicsObjectId`, including a step/frame counter and per-body kinematic state.

#### Scenario: Capture over stable ids

- **WHEN** a snapshot is captured from a running world
- **THEN** it SHALL record the step counter and per-body transform, linear velocity, angular velocity, and sleep state keyed by `PhysicsObjectId`

#### Scenario: Snapshot scope is selectable

- **WHEN** a snapshot is requested with the default scope
- **THEN** it SHALL include dynamic and kinematic bodies and MAY omit static state; a full scope SHALL include all objects

### Requirement: Restore world state

A physics world SHALL restore a previously captured snapshot, replacing the simulation state while preserving object identity so that existing handles remain valid.

#### Scenario: Restore reproduces captured state

- **WHEN** a snapshot is restored
- **THEN** subsequent reads SHALL report the captured kinematic state for the captured ids

#### Scenario: Handles survive restore

- **WHEN** a snapshot is restored into the same world
- **THEN** `PhysicsObjectId` handles that were valid before the restore SHALL remain valid

#### Scenario: Mismatched snapshot is rejected

- **WHEN** a snapshot produced by an incompatible world or version is restored
- **THEN** the restore SHALL fail safely without partially corrupting the world

### Requirement: Snapshots support editor replay

The engine SHALL allow buffering snapshots so the editor can pause, single-step, and replay simulation, without the snapshot containing any render or backend types.

#### Scenario: Buffer and replay

- **WHEN** the editor buffers snapshots over several steps and restores an earlier one
- **THEN** the world SHALL resume from the restored state and stepping SHALL continue from the restored step counter

### Requirement: Rollback uses snapshot restore

The world SHALL support rollback by restoring a captured snapshot. Re-simulation from a confirmed snapshot is reserved for the future `Exact` mode (`physics-determinism`) and SHALL be reported unavailable on the delivered `Fast` mode.

#### Scenario: Restore rolls back state

- **WHEN** a previously captured snapshot is restored
- **THEN** subsequent reads SHALL report the captured state and stepping SHALL continue from the captured step counter

#### Scenario: Re-simulation is reserved

- **WHEN** re-simulation-based rollback is requested on the delivered `Fast` mode
- **THEN** the world SHALL report it unavailable and use snapshot restore
