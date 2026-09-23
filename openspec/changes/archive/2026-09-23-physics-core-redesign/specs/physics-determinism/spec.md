## ADDED Requirements

### Requirement: Selectable simulation math mode interface

The engine SHALL expose a selectable `PhysicsMathMode` with at least `Fast` and `Exact`, and every backend/world SHALL report its active mode together with whether that mode is deterministic, so that consumers can decide whether determinism-dependent features are available.

#### Scenario: Query the active mode

- **WHEN** a consumer queries the world's capabilities
- **THEN** it SHALL receive the active `PhysicsMathMode` and a deterministic flag for that mode

#### Scenario: Requesting an unavailable mode is rejected

- **WHEN** a world is requested in `Exact` mode and no deterministic backend is registered
- **THEN** the engine SHALL report the mode as unavailable rather than silently creating a `Fast` world

### Requirement: Fast mode is the delivered mode and is not deterministic

This change SHALL deliver the `Fast` mode (native `float`), and a `Fast`-mode world SHALL report that determinism is unavailable so that determinism-dependent features are not silently used.

#### Scenario: Fast reports non-deterministic

- **WHEN** the shipped `Fast` backend is queried for capabilities
- **THEN** it SHALL report that determinism is unavailable

#### Scenario: Determinism-dependent features are gated

- **WHEN** a determinism-dependent feature (such as rollback re-simulation or lockstep) is requested on a `Fast` world
- **THEN** the engine SHALL report determinism as unavailable rather than claim it

### Requirement: Reserved cross-platform determinism contract for Exact

`Exact` mode SHALL, once a deterministic backend is registered, produce bit-exact results across the supported platforms for SkyEngine's own builds given the same initial state and the same ordered input sequence, independent of frame rate and wall-clock time. This requirement is **reserved**: it is not delivered by this change, which only provides the interface and the `Fast` implementation.

#### Scenario: Reserved for a future backend

- **WHEN** no deterministic backend is registered
- **THEN** `Exact` SHALL remain unavailable and this requirement SHALL be documented as reserved

#### Scenario: Satisfied when an Exact backend ships

- **WHEN** a deterministic backend is registered and a world is created in `Exact` mode
- **THEN** the same initial state and ordered input sequence SHALL yield identical object states across supported platforms

### Requirement: Reserved determinism validation

The reserved `Exact` contract SHALL be covered, in the change that implements a deterministic backend, by a reproducibility test comparing simulation and event traces across the supported platforms. This is **reserved** with the `Exact` implementation.

#### Scenario: Reserved with the implementation

- **WHEN** the deterministic backend is implemented
- **THEN** a cross-platform trace test SHALL be added to validate the contract
