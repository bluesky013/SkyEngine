# network-lockstep Specification

## Purpose
TBD - created by archiving change add-network-lockstep. Update Purpose after archive.
## Requirements
### Requirement: Peers exchange input frames and simulate deterministically

Peers SHALL exchange input frames for each tick on a reliable-ordered channel and SHALL advance a fixed simulation tick without transmitting steady-state entity state.

#### Scenario: Same inputs produce the same tick

- **WHEN** all peers have received the input frame for a tick
- **THEN** each peer SHALL advance the simulation for that tick using only those inputs

### Requirement: Input frames are ordered and complete

The lockstep channel SHALL guarantee that every peer applies the same set of inputs for a given tick in the same order.

#### Scenario: No peer advances early

- **WHEN** a peer has not received the input frame for a tick
- **THEN** it SHALL wait rather than advance the simulation for that tick

### Requirement: Input delay is configurable

The system SHALL support a configurable input delay so inputs are available before their tick, and SHALL allow latency-sensitive genres to enable input prediction with rollback instead.

#### Scenario: Delay applied before tick

- **WHEN** an input is produced locally for a future tick within the input delay
- **THEN** it SHALL be scheduled and available before that tick is simulated

### Requirement: A reconnecting peer resynchronizes

A peer that rejoins after missing input frames SHALL obtain authoritative state and resynchronize before participating in lockstep again.

#### Scenario: Missed inputs recovered

- **WHEN** a peer reconnects after missing inputs
- **THEN** it SHALL resynchronize from authoritative state rather than replaying gaps

