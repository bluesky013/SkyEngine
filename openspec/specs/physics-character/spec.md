# physics-character Specification

## Purpose
TBD - created by archiving change physics-core-redesign. Update Purpose after archive.
## Requirements
### Requirement: Character controller descriptor

The physics module SHALL describe a character controller as plain data comprising a capsule (radius and height), step height, slope limit, gravity, up axis, and an initial transform, and the world SHALL allow characters to be addressed by `PhysicsObjectId`.

#### Scenario: Describe a character

- **WHEN** a character descriptor is created
- **THEN** it SHALL reference engine data only and SHALL be created through the world, returning a handle

#### Scenario: Runtime capsule change

- **WHEN** the capsule dimensions of a live character are changed
- **THEN** the world SHALL rebuild the underlying character collision without losing its handle

### Requirement: Character movement semantics

The character controller SHALL accept a displacement for the next movement step and SHALL report whether the displacement was accepted, while resolving collisions against the simulated world.

#### Scenario: Move is applied

- **WHEN** a displacement is submitted to a grounded character
- **THEN** the character SHALL be moved by the resolved displacement against collisions and SHALL report acceptance

#### Scenario: Move before attach

- **WHEN** a displacement is submitted before the character is attached to a world
- **THEN** the controller SHALL report it was not applied rather than crash

### Requirement: Grounded and world-transform queries

The character controller SHALL report whether it is grounded and SHALL allow reading and writing its world transform.

#### Scenario: Grounded after landing

- **WHEN** a character falls onto walkable geometry and settles
- **THEN** the controller SHALL report grounded

#### Scenario: Read back transform

- **WHEN** the character has moved under simulation
- **THEN** reading its world transform SHALL reflect the simulated position

### Requirement: Baseline character implementation

The active backend SHALL provide a baseline capsule character controller matching this contract, and the engine SHALL function without it by treating characters as unavailable.

#### Scenario: Backend provides a character

- **WHEN** a character is created through a backend that supports characters
- **THEN** it SHALL move, report grounded state, and respect step height and slope limit

#### Scenario: Backend lacks characters

- **WHEN** characters are requested from a backend without character support
- **THEN** the world SHALL report the capability as unavailable

