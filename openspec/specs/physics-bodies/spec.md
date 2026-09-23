# physics-bodies Specification

## Purpose
TBD - created by archiving change physics-core-redesign. Update Purpose after archive.
## Requirements
### Requirement: Body kinds

The physics module SHALL describe a body as exactly one of static, dynamic, or kinematic, and the world SHALL apply the matching simulation behavior per kind.

#### Scenario: Static body does not move under simulation

- **WHEN** a static body is added and the world steps
- **THEN** the body SHALL remain at its authored transform unless explicitly moved

#### Scenario: Dynamic body responds to forces

- **WHEN** a dynamic body is added under gravity and the world steps
- **THEN** the body SHALL move according to the simulated forces

#### Scenario: Kinematic body follows authored motion

- **WHEN** a kinematic body's transform is set between steps
- **THEN** the body SHALL move to that transform and push dynamic bodies per the backend's kinematic behavior

### Requirement: Backend-neutral shape descriptors

The physics module SHALL describe collision shapes as plain data, covering at least box, sphere, capsule, heightfield, and triangle mesh, and SHALL allow a compound shape composed of multiple descriptors; shape data SHALL NOT contain backend types.

#### Scenario: Describe a shape without a backend

- **WHEN** a body descriptor carries a shape description
- **THEN** it SHALL reference engine data only and SHALL be accepted without any backend header

#### Scenario: Compound shape

- **WHEN** a body descriptor carries a compound of several shape descriptions
- **THEN** the backend SHALL be able to build a collision shape combining them

#### Scenario: Heightfield and triangle mesh remain distinct

- **WHEN** a heightfield or triangle-mesh shape is described
- **THEN** the physics module SHALL preserve the distinction and pass it to the backend without loading a render mesh

### Requirement: Mass, inertia, and damping

The physics module SHALL allow a body descriptor to specify mass, linear damping, and angular damping, with inertia either computed by the backend from the shape or supplied explicitly.

#### Scenario: Damping is applied

- **WHEN** a body is created with linear and angular damping
- **THEN** the backend SHALL apply those damping values to the simulated body

#### Scenario: Mass drives inertia

- **WHEN** a dynamic body is created with a mass and no explicit inertia
- **THEN** the backend SHALL compute its local inertia from the collision shape

### Requirement: Sleep control

The physics module SHALL allow a body to specify whether sleeping is enabled and an initial sleep state, and SHALL allow a sleeping body to be woken by external input.

#### Scenario: Body falls asleep when idle

- **WHEN** a dynamic body with sleeping enabled comes to rest and the world steps
- **THEN** it SHALL be allowed to enter a sleeping state

#### Scenario: External input wakes a body

- **WHEN** a force, impulse, or transform is applied to a sleeping body
- **THEN** the body SHALL wake and resume simulation

### Requirement: Continuous collision detection configuration

The physics module SHALL allow continuous collision detection to be requested per body and SHALL expose whether the active backend supports it.

#### Scenario: CCD is opt-in

- **WHEN** a body is created without requesting CCD
- **THEN** the backend SHALL NOT apply continuous collision detection to it

#### Scenario: Unsupported CCD is reported

- **WHEN** a body requests CCD and the backend does not support it
- **THEN** the world SHALL report the capability as unavailable rather than silently mis-simulating

