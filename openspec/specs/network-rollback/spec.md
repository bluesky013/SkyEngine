# network-rollback Specification

## Purpose
TBD - created by archiving change add-network-lockstep. Update Purpose after archive.
## Requirements
### Requirement: Simulation state can be captured and restored deterministically

The system SHALL be able to capture the deterministic simulation state at a tick and restore it exactly, over stable entity ids.

#### Scenario: Restore reproduces state

- **WHEN** state is captured at a tick and later restored
- **THEN** the simulation SHALL continue identically to the original as if it had never advanced past that tick

### Requirement: Rollback resimulates corrected frames

On receiving a corrected or predicted input for a past tick, the system SHALL roll back to that tick, apply the input, and resimulate the frames up to the present.

#### Scenario: Late input corrects the past

- **WHEN** a peer learns that a predicted remote input was wrong for a past tick
- **THEN** it SHALL roll back to that tick, apply the correct input, and resimulate to the present

### Requirement: Rollback depth is bounded

Rollback SHALL be limited to a configured maximum number of frames, and SHALL NOT grow unbounded.

#### Scenario: Excessive lag clamps rollback

- **WHEN** a correction would require rolling back further than the configured limit
- **THEN** the system SHALL resynchronize from authoritative state instead of rolling back

### Requirement: Rollback is used only in deterministic mode

Rollback SHALL only be available when the simulation is running in the deterministic mode; it SHALL NOT be used over the state-replication path.

#### Scenario: Rollback unavailable without determinism

- **WHEN** the simulation is not in deterministic mode
- **THEN** rollback SHALL be rejected

